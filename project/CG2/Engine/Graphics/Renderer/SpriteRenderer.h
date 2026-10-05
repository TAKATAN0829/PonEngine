#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

class ShaderCompiler;
class DescriptorHeapManager;

class SpriteRenderer {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(
		ID3D12Device* device,
		ShaderCompiler* shaderCompiler,
		DescriptorHeapManager* srvDescriptorHeap);

	//=============================================================================================//
	// 描画前設定

	void PreDraw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
		D3D12_INDEX_BUFFER_VIEW* indexBufferView);

private:

	//=============================================================================================//
	// SRVDescriptorHeap

	DescriptorHeapManager* srvDescriptorHeap_ = nullptr;

	//=============================================================================================//
	// RootSignature

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	//=============================================================================================//
	// PipelineState

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
