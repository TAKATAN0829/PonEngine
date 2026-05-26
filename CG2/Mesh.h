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

class Mesh {
public:

	//=============================================================================================//
	// 初期化処理

	void InitializeSphere (
		ID3D12Device* device,
		ResourceManager* resourceManager);

	//=============================================================================================//
	// VertexBufferView取得

	D3D12_VERTEX_BUFFER_VIEW* GetVertexBufferView ();

	//=============================================================================================//
	// IndexBufferView取得

	D3D12_INDEX_BUFFER_VIEW* GetIndexBufferView ();

	//=============================================================================================//
	// Index数取得

	uint32_t GetIndexCount ();

private:

	//=============================================================================================//
	// Sphere生成

	void CreateSphere ();

private:

	static const uint32_t kSubdivision = 16;
	static const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
	static const uint32_t kIndexCount = kSubdivision * kSubdivision * 6;

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