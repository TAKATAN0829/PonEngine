#include "FrameProfiler.h"
#include <exception>
#include "ModelManager.h"
#include <utility>

// 自作
#include "ModelCommon.h"

//=============================================================================================//
// インスタンス取得


ModelManager* ModelManager::GetInstance() {

	static ModelManager instance;

	return &instance;
}

//=============================================================================================//
// 初期化処理


void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {

	modelCommon_ = new ModelCommon();

	modelCommon_->Initialize(dxCommon, srvManager);
}

//=============================================================================================//
// 終了処理


void ModelManager::Finalize() {

	models_.clear();

	retiredModels_.clear();

	delete modelCommon_;

	modelCommon_ = nullptr;
}

//=============================================================================================//
// モデルファイル読み込み


void ModelManager::LoadModel(const std::string& filePath) {

	if (models_.contains(filePath)) {

		return;
	}

	FrameProfiler::Scope loadTime(FrameProfiler::Section::ModelLoad);

	std::unique_ptr<Model> model = std::make_unique<Model>();

	model->Initialize(modelCommon_, kDirectoryPath, filePath);

	models_.insert(std::make_pair(filePath, std::move(model)));
}

// 壊れたアセットはキャッシュに登録せず、UIへ理由を返す。
bool ModelManager::TryLoadModel(const std::string& filePath, std::string& error) {
	error.clear();

	try {
		LoadModel(filePath);
	}
	catch (const std::exception& failure) {
		error = failure.what();

		return false;
	}

	return true;
}

//=============================================================================================//
// Asset参照の更新

void ModelManager::RemapAssetPaths(const AssetPathChange& change) {
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

	if (models_.contains(filePath)) {

		return models_.at(filePath).get();
	}

	return nullptr;
}
