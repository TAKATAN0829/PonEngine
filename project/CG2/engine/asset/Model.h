#pragma once

// C++
#include <string>
#include <vector>
#include <memory>

// 自作
#include "EngineStructs.h"
#include "Mesh.h"
#include "Material.h"

class ModelCommon;
struct aiNode;

class Model {
public:

	//=============================================================================================//
	// ノード

	struct Node {
		Matrix4x4 localMatrix = MathUtility::MakeIdentity4x4();
		std::string name;
		std::vector<Node> children;
	};

	//=============================================================================================//
	// モデルデータ

	struct ModelData {
		std::vector<Mesh::VertexData> vertices;
		MaterialData material;
		Node rootNode;
	};

public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);

	//=============================================================================================//
	// 描画処理

	void Draw (Material* instanceMaterial = nullptr);

	//=============================================================================================//
	// Getter

	Material* GetMaterial () const { return material_.get (); }

	const Node& GetRootNode () const { return modelData_.rootNode; }
	// 読込時に求めたメッシュの範囲。画面上の選択判定に使用する。
	const AABB& GetLocalBounds() const { return localBounds_; }

private:

	//=============================================================================================//
	// モデルファイル読み込み

	static ModelData LoadModelFile (const std::string& directoryPath, const std::string& filename);

	//=============================================================================================//
	// Node読み込み

	static Node ReadNode (aiNode* node);

private:

	//=============================================================================================//
	// ModelCommon

	ModelCommon* modelCommon_ = nullptr;

	//=============================================================================================//
	// モデルデータ

	ModelData modelData_;
	AABB localBounds_{};

	//=============================================================================================//
	// Mesh

	std::unique_ptr<Mesh> mesh_;

	//=============================================================================================//
	// Material

	std::unique_ptr<Material> material_;
};
