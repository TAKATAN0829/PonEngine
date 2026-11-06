#pragma once

#include "AssetPathChange.h"

// C++
#include <string>
#include <vector>
#include <memory>

// 自作
#include "EngineStructs.h"
#include "Mesh.h"
#include "Material.h"
#include "ModelAnimation.h"
#include "ModelImportInfo.h"

class ModelCommon;

struct aiNode;

class Model {
public:

	//=============================================================================================//
	// ノード

	struct Node {
		Node();

		Matrix4x4 localMatrix;

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
			uint32_t firstIndex;
			uint32_t indexCount;
			uint32_t materialIndex;
		};

		std::vector<DrawRange> ranges;

		bool isAnimated;
	};

public:

	//=============================================================================================//
	// 初期化処理

	bool TryInitialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename, std::string& error);

	//=============================================================================================//
	// 描画処理

	void Draw(Material* instanceMaterial = nullptr, const std::vector<std::unique_ptr<Material>>* slots = nullptr, Mesh* animatedMesh = nullptr);

	//=============================================================================================//
	// Getter

	Material* GetMaterial () const;

	const Node& GetRootNode () const;

	// 読込時に求めたメッシュの範囲。画面上の選択判定に使用する。
	const AABB& GetLocalBounds() const;

	// 元データは共有し、編集値と変形用メッシュはObject3dが個別に所有する。
	const std::vector<ModelMaterialInfo>& GetMaterials() const;

	uint32_t GetTextureIndex(size_t slot) const;

	const std::vector<Mesh::VertexData>& GetVertices() const;

	const ModelAnimation* GetAnimation() const;

	//=============================================================================================//
	// Asset参照の更新

	// 既存のGPUマテリアルを保持し、Inspectorへ公開するパスだけを更新する。
	void RemapAssetPaths(const AssetPathChange& change);

private:

	//=============================================================================================//
	// モデルファイル読み込み

	static bool LoadModelFile(const std::string& directoryPath, const std::string& filename, ModelData& modelData, std::string& error);

	//=============================================================================================//
	// Node読み込み

	static Node ReadNode (aiNode* node);

private:

	//=============================================================================================//
	// ModelCommon

	ModelCommon* modelCommon_ = nullptr;

	//=============================================================================================//
	// モデルデータ

	ModelData modelData_{};

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
