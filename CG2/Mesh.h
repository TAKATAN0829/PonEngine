#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>

// 自作
#include "EngineStructs.h"
#include "ResourceManager.h"

enum class MeshType {
	kSphere,
	kPlane,
	kTriangle,
};

class Mesh {
public:
	//=============================================================================================//
	// 初期化

	void Initialize (ID3D12Device* device, ResourceManager* resourceManager, MeshType meshType);

	//=============================================================================================//
	// 描画処理

	void Draw (ID3D12GraphicsCommandList* commandList);

	//=============================================================================================//
	// 頂点BufferView取得

	D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView ();

	//=============================================================================================//
	// IndexBufferView取得

	D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView ();

	//=============================================================================================//
	// Index数取得

	uint32_t GetIndexCount ();

private:

	//=============================================================================================//
	// Sphere初期化

	void InitializeSphere (ID3D12Device* device, ResourceManager* resourceManager);

	//=============================================================================================//
	// Plane初期化

	void InitializePlane (ID3D12Device* device, ResourceManager* resourceManager);

	//=============================================================================================//
	// Triangle初期化

	void InitializeTriangle (ID3D12Device* device, ResourceManager* resourceManager);

	//=============================================================================================//
	// 頂点Resource生成

	void CreateVertexResource (ID3D12Device* device, ResourceManager* resourceManager);

	//=============================================================================================//
	// IndexResource生成

	void CreateIndexResource (ID3D12Device* device,	ResourceManager* resourceManager);

	//=============================================================================================//
	// Sphere生成

	void CreateSphere ();

	//=============================================================================================//
	// Plane生成

	void CreatePlane ();

	//=============================================================================================//
	// Triangle生成

	void CreateTriangle ();

private:

	//=============================================================================================//
	// Sphere分割数

	static const uint32_t kSubdivision = 16;

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