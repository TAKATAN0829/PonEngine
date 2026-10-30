#include "FrameProfiler.h"
#include <exception>
#include "ModelManager.h"

// 自作
#include "ModelCommon.h"

//=================================================================================================//
// インスタンス取得

ModelManager* ModelManager::GetInstance() {

	static ModelManager instance;

	return &instance;
}

//=================================================================================================//
// 初期化処理

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {

	modelCommon_ = new ModelCommon();

	modelCommon_->Initialize(dxCommon, srvManager);
}

//=================================================================================================//
// 終了処理

void ModelManager::Finalize() {

	models_.clear();

	delete modelCommon_;

	modelCommon_ = nullptr;
}

//=================================================================================================//
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

//=================================================================================================//
// モデル検索

Model* ModelManager::FindModel(const std::string& filePath) {

	if (models_.contains(filePath)) {

		return models_.at(filePath).get();
	}

	return nullptr;
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
