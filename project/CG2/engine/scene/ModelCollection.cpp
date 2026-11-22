#include "ModelCollection.h"

#include <utility>

//=================================================================================================//
// 初期化処理

void ModelCollection::Initialize() {
	models_.clear();
	nextId_ = 1;
}

//=================================================================================================//
// 終了処理

void ModelCollection::Finalize() {
	models_.clear();
}

//=================================================================================================//
// モデル追加

ModelId ModelCollection::AddModel(const std::string& name, const std::string& modelFile, const std::string& textureFile) {
	//===========================================//
	// モデルデータ
	//===========================================//

	SceneModel model;
	model.id = nextId_;
	model.name = name;
	model.modelFile = modelFile;
	model.textureFile = textureFile;
	model.transform = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};
	model.color = {1.0f, 1.0f, 1.0f, 1.0f};
	model.enableLighting = true;
	model.shininess = 40.0f;
	model.useRootNodeMatrix = true;

	//===========================================//
	// 一覧への登録
	//===========================================//

	const ModelId id = model.id;
	models_.push_back(std::move(model));
	++nextId_;

	return id;
}

//=================================================================================================//
// モデル検索

SceneModel* ModelCollection::FindModel(ModelId id) {
	for (SceneModel& model : models_) {
		if (model.id == id) {
			return &model;
		}
	}

	return nullptr;
}

//=================================================================================================//
// モデル一覧の取得

const std::vector<SceneModel>& ModelCollection::GetModels() const {
	return models_;
}
