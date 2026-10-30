#pragma once

// C++
#include <string>
#include <vector>
#include <memory>

// 自作
#include "EngineStructs.h"
#include "Mesh.h"
#include "Material.h"
#include "ModelAnimation.h"

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
		std::vector<ModelMaterialInfo> materials;
		std::vector<uint32_t> textureIndices;
		struct DrawRange {
			uint32_t firstIndex = 0;
			uint32_t indexCount = 0;
			uint32_t materialIndex = 0;
		};
		std::vector<DrawRange> ranges;
		bool animated = false;
	};

public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);

	//=============================================================================================//
	// 描画処理

	void Draw(Material* instanceMaterial = nullptr, const std::vector<std::unique_ptr<Material>>* slots = nullptr, Mesh* animatedMesh = nullptr);

	//=============================================================================================//
	// Getter

	Material* GetMaterial () const { return material_.get (); }

	const Node& GetRootNode () const { return modelData_.rootNode; }
	// 読込時に求めたメッシュの範囲。画面上の選択判定に使用する。
	const AABB& GetLocalBounds() const { return localBounds_; }
	// 元データは共有し、編集値と変形用メッシュはObject3dが個別に所有する。
	const std::vector<ModelMaterialInfo>& GetMaterials() const { return modelData_.materials; }
	uint32_t GetTextureIndex(size_t slot) const { return modelData_.textureIndices.at(slot); }
	const std::vector<Mesh::VertexData>& GetVertices() const { return modelData_.vertices; }
	const ModelAnimation* GetAnimation() const { return animation_.get(); }

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
	std::vector<std::unique_ptr<Material>> materials_; // インスタンスを指定しない場合の既定値。
	std::unique_ptr<ModelAnimation> animation_; // ノードとクリップの共有データ。
};
