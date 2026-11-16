#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "EngineStructs.h"
#include "Object3d.h"

using ModelId = uint64_t;

// モデル1個のデータと描画用Object3dを所有する。
struct SceneModel {
	ModelId id;
	std::string name;
	TransformData transform;
	Vector4 color;
	bool enableLighting;
	float shininess;
	bool useRootNodeMatrix;
	std::unique_ptr<Object3d> object;
};

class Scene {
public:

	//===========================================//
	// 初期化処理
	//===========================================//

	void Initialize(Object3dCommon* object3dCommon);

	//===========================================//
	// 更新処理
	//===========================================//

	void Update(const Matrix4x4& viewProjectionMatrix);

	//===========================================//
	// 描画処理
	//===========================================//

	void Draw();

	//===========================================//
	// 終了処理
	//===========================================//

	void Finalize();

	//===========================================//
	// モデル追加
	//===========================================//

	ModelId AddModel(const std::string& name, const std::string& modelFile, const std::string& textureFile);

	//===========================================//
	// モデル検索
	//===========================================//

	SceneModel* FindModel(ModelId id);

	//===========================================//
	// モデル一覧の取得
	//===========================================//

	const std::vector<SceneModel>& GetModels() const;

private:

	//===========================================//
	// 描画機能
	//===========================================//

	Object3dCommon* object3dCommon_;

	//===========================================//
	// モデル一覧
	//===========================================//

	std::vector<SceneModel> models_;
	ModelId nextId_;
};
