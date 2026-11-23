#pragma once

#include <memory>
#include <string>
#include <unordered_map>

#include "ModelCollection.h"
#include "Object3d.h"

class ModelRenderer {
public:

	//===========================================//
	// 初期化処理
	//===========================================//

	void Initialize(Object3dPipeline* object3dPipeline);

	//===========================================//
	// 更新処理
	//===========================================//

	void Update(const ModelCollection& models, const Matrix4x4& viewProjectionMatrix);

	//===========================================//
	// 描画処理
	//===========================================//

	void Draw(const ModelCollection& models);

	//===========================================//
	// 終了処理
	//===========================================//

	void Finalize();

	//===========================================//
	// モデル追加
	//===========================================//

	void AddModel(const SceneModel& model);

	//===========================================//
	// モデル検索
	//===========================================//

	Object3d* FindObject(ModelId id);

	//===========================================//
	// モデル一覧に合わせた描画物の更新
	//===========================================//

	void Refresh(const ModelCollection& models);

private:
	//===========================================//
	// 描画するオブジェクトの情報
	//===========================================//

	struct RenderObject {
		std::string modelFile;
		std::string textureFile;
		std::unique_ptr<Object3d> object;
	};

	//===========================================//
	// 描画機能
	//===========================================//

	Object3dPipeline* object3dPipeline_;

	//===========================================//
	// 描画用Object3d
	//===========================================//

	std::unordered_map<ModelId, RenderObject> objects_;
};
