#include "FrameProfiler.h"
#include <stdexcept>
#include "ModelManager.h"
#include <utility>

// 自作
#include "ModelCommon.h"

//=============================================================================================//
// インスタンス取得


ModelManager* ModelManager::GetInstance() {
	// 全体で共有する一つのインスタンスを返す。

	static ModelManager instance;

	return &instance;
}

//=============================================================================================//
// 初期化処理


void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	// モデル読込で使う共通描画機能の参照を保持する。

	modelCommon_ = new ModelCommon();

	modelCommon_->Initialize(dxCommon, srvManager);
}

//=============================================================================================//
// 終了処理


void ModelManager::Finalize() {
	// 登録したモデルを終了し、キャッシュを空にする。

	models_.clear();

	retiredModels_.clear();

	delete modelCommon_;

	modelCommon_ = nullptr;
}

//=============================================================================================//
// モデルファイル読み込み


void ModelManager::LoadModel(const std::string& filePath) {
	std::string error;
	if (!TryLoadModel(filePath, error)) {
		throw std::runtime_error(error);
	}
}

// 壊れたアセットはキャッシュに登録せず、UIへ理由を返す。
bool ModelManager::TryLoadModel(const std::string& filePath, std::string& error) {
	// 今回の読み込み結果だけを呼び出し元へ返す。
	error.clear();

	// 同じパスのモデルが読み込み済みなら、キャッシュを使う。
	if (models_.contains(filePath)) {
		return true;
	}

	// ディスクからの読み込みと初期化にかかった時間を計測する。
	FrameProfiler::Scope loadTime(FrameProfiler::Section::ModelLoad);
	auto model = std::make_unique<Model>();

	// 基準フォルダと相対パスを Model に渡し、ファイル解析と初期化を任せる。
	if (!model->TryInitialize(modelCommon_, kDirectoryPath, filePath, error)) {
		return false;
	}

	// 初期化が成功したモデルだけを登録し、次回は同じパスで再利用する。
	models_.emplace(filePath, std::move(model));
	return true;
}

//=============================================================================================//
// Asset参照の更新

void ModelManager::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	std::vector<std::string> changedKeys;

	for (const std::pair<const std::string, std::unique_ptr<Model>>& entry : models_) {
		entry.second->RemapAssetPaths(change);

		if (change.RemapRelative(entry.first) != entry.first) {
			changedKeys.push_back(entry.first);
		}
	}

	for (const std::string& key : changedKeys) {
		const std::string destination = change.RemapRelative(key);

		std::map<std::string, std::unique_ptr<Model>>::node_type node = models_.extract(key);

		if (models_.contains(destination)) {
			retiredModels_.push_back(std::move(models_.at(destination)));

			models_.erase(destination);
		}

		node.key() = destination;

		models_.insert(std::move(node));
	}
}

//=============================================================================================//
// モデル検索


Model* ModelManager::FindModel(const std::string& filePath) {
	// 読込済みのモデルをパスで検索する。

	if (models_.contains(filePath)) {

		return models_.at(filePath).get();
	}

	return nullptr;
}
