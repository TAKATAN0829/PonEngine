#include "ModelRenderer.h"
#include "ModelManager.h"
#include "Object3dPipeline.h"

#include <cassert>
#include <utility>

//=================================================================================================//
// 初期化処理

void ModelRenderer::Initialize(Object3dPipeline* object3dPipeline) {
	assert(object3dPipeline != nullptr);

	object3dPipeline_ = object3dPipeline;
	objects_.clear();
}

//=================================================================================================//
// モデル追加

void ModelRenderer::AddModel(const SceneModel& model) {

	//===========================================//
	// 登録確認
	//===========================================//

	if (FindObject(model.id) != nullptr) {
		return;
	}

	//===========================================//
	// Object3d生成
	//===========================================//

	// データ読み込み
	ModelManager::GetInstance()->LoadModel(model.modelFile);

	// 生成
	std::unique_ptr<Object3d> object = std::make_unique<Object3d>();
	object->Initialize(object3dPipeline_);
	object->SetModel(
		ModelManager::GetInstance()->FindModel(model.modelFile)
	);

	// テクスチャ設定
	if (!model.textureFile.empty()) {
		object->SetTexture(model.textureFile);
	}

	//===========================================//
	// 一覧への登録
	//===========================================//

	objects_.emplace(model.id, std::move(object));
}

//=================================================================================================//
// モデル検索

Object3d* ModelRenderer::FindObject(ModelId id) {
	auto found = objects_.find(id);

	if (found == objects_.end()) {
		return nullptr;
	}

	return found->second.get();
}

//=================================================================================================//
// 更新処理

void ModelRenderer::Update(
	const ModelCollection& models,
	const Matrix4x4& viewProjectionMatrix
) {
	for (const SceneModel& model : models.GetModels()) {

		// 描画対象の取得
		Object3d* object = FindObject(model.id);

		if (object == nullptr) {
			continue;
		}

		// データの反映
		object->SetTransform(model.transform);
		object->SetColor(model.color);
		object->SetEnableLighting(model.enableLighting);
		object->SetShininess(model.shininess);
		object->SetUseRootNodeMatrix(model.useRootNodeMatrix);

		// 行列の更新
		object->Update(viewProjectionMatrix);
	}
}

//=================================================================================================//
// 描画処理

void ModelRenderer::Draw(const ModelCollection& models) {
	object3dPipeline_->DrawSettingCommon();

	for (const SceneModel& model : models.GetModels()) {
		Object3d* object = FindObject(model.id);

		if (object != nullptr) {
			object->Draw();
		}
	}
}

//=================================================================================================//
// 終了処理

void ModelRenderer::Finalize() {
	objects_.clear();
	object3dPipeline_ = nullptr;
}