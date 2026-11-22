#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "EngineStructs.h"

// ModelIdはuint64_t型として扱う
using ModelId = uint64_t;

// 一つのModelが持つ情報
// モデル1個のデータを持つ。
struct SceneModel {
	ModelId id;
	std::string name;

	// 使用するファイル
	std::string modelFile;
	std::string textureFile;

	TransformData transform;
	Vector4 color;
	bool enableLighting;
	float shininess;
	bool useRootNodeMatrix;
};

/// <summary>
/// シーンに存在するModel一覧
/// </summary>
class ModelCollection {
public:

	//===========================================//
	// 初期化処理
	//===========================================//

	void Initialize();

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
	// モデル一覧
	//===========================================//

	std::vector<SceneModel> models_;
	ModelId nextId_;
};
