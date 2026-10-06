#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>
#include <string>

// 自作
#include "EngineStructs.h"
#include "ResourceManager.h"

class DirectXCommon;

enum class MeshType {
	kSphere,
	kPlane,
	kTriangle,
};

class Mesh {
public:
	//=============================================================================================//
	// 初期化

	void Initialize (DirectXCommon* dxCommon, MeshType meshType);

	//=============================================================================================//
	// 初期化(OBJモデル)

	void Initialize (DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename);

	//=============================================================================================//
	// 描画処理

	void Draw (uint32_t instanceCount = 1);

private:

	//=============================================================================================//
	// Sphere初期化

	void InitializeSphere ();

	//=============================================================================================//
	// Plane初期化

	void InitializePlane ();

	//=============================================================================================//
	// Triangle初期化

	void InitializeTriangle ();

	//=============================================================================================//
	// 頂点Resource生成

	void CreateVertexResource ();

	//=============================================================================================//
	// IndexResource生成

	void CreateIndexResource ();

	//=============================================================================================//
	// Sphere生成

	void CreateSphere ();

	//=============================================================================================//
	// Plane生成

	void CreatePlane ();

	//=============================================================================================//
	// Triangle生成

	void CreateTriangle ();

	//=============================================================================================//
	// OBJファイル読み込み

	static ModelData LoadObjFile (const std::string& directoryPath, const std::string& filename);

private:

	//=============================================================================================//
	// Sphere分割数

	static const uint32_t kSubdivision = 16;

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// Vertex数

	uint32_t vertexCount_ = 0;

	//=============================================================================================//
	// Index数

	uint32_t indexCount_ = 0;

	//=============================================================================================//
	// VertexResource

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;

	//=============================================================================================//
	// VertexBufferView

	D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

	//=============================================================================================//
	// VertexData

	VertexData* vertexData_ = nullptr;

	//=============================================================================================//
	// IndexResource

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;

	//=============================================================================================//
	// IndexBufferView

	D3D12_INDEX_BUFFER_VIEW indexBufferView_{};

	//=============================================================================================//
	// IndexData

	uint32_t* indexData_ = nullptr;
};