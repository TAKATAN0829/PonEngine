#include "Scene.h"
#include "ModelManager.h"
#include "Object3dCommon.h"

#include <cassert>
#include <utility>

//=================================================================================================//
// 初期化処理

void Scene::Initialize(Object3dCommon* object3dCommon) {
	assert(object3dCommon != nullptr);
	object3dCommon_ = object3dCommon;
	models_.clear();
	nextId_ = 1;
}

//=================================================================================================//
// 更新処理

void Scene::Update(const Matrix4x4& viewProjectionMatrix) {
	for (SceneModel& model : models_) {
		// データの反映
		model.object->SetTransform(model.transform);
		model.object->SetColor(model.color);
		model.object->SetEnableLighting(model.enableLighting);
		model.object->SetShininess(model.shininess);
		model.object->SetUseRootNodeMatrix(model.useRootNodeMatrix);

		// 行列の更新
		model.object->Update(viewProjectionMatrix);
	}
}

//=================================================================================================//
// 描画処理

void Scene::Draw() {
	object3dCommon_->DrawSettingCommon();

	for (SceneModel& model : models_) {
		model.object->Draw();
	}
}

//=================================================================================================//
// 終了処理

void Scene::Finalize() {
	// unique_ptrが各Object3dを解放する。
	models_.clear();
	object3dCommon_ = nullptr;
}

//=================================================================================================//
// モデル追加

ModelId Scene::AddModel(const std::string& name, const std::string& modelFile, const std::string& textureFile) {
	//===========================================//
	// モデルデータ
	//===========================================//

	SceneModel model;
	model.id = nextId_;
	model.name = name;
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
	// 描画用Object3d
	//===========================================//

	// データ読み込み
	ModelManager::GetInstance()->LoadModel(modelFile);

	// 生成
	model.object = std::make_unique<Object3d>();
	model.object->Initialize(object3dCommon_);
	model.object->SetModel(ModelManager::GetInstance()->FindModel(modelFile));
	if (!textureFile.empty()) {
		model.object->SetTexture(textureFile);
	}

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

SceneModel* Scene::FindModel(ModelId id) {
	for (SceneModel& model : models_) {
		if (model.id == id) {
			return &model;
		}
	}

	return nullptr;
}

//=================================================================================================//
// モデル一覧の取得

const std::vector<SceneModel>& Scene::GetModels() const {
	return models_;
}
