#pragma once
// WRL
#include <wrl.h>

// DirectX
#include <d3d12.h>

// DXC
#include <dxcapi.h>

// C++
#include <string>
#include <cstdint>

// 自作
#include "DescriptorHeapManager.h"
#include "ShaderCompiler.h"
#include "TextureManager.h"
#include "ResourceManager.h"
#include "PipelineManager.h"

class GraphicsSystem {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (ID3D12Device* device);

	//=============================================================================================//
	// 終了処理

	void Finalize ();

	//=============================================================================================//
	// SRVDescriptorHeap取得

	DescriptorHeapManager* GetSRVDescriptorHeap ();

	//=============================================================================================//
	// ShaderCompiler取得

	ShaderCompiler* GetShaderCompiler ();

	//=============================================================================================//
	// TextureManager取得

	TextureManager* GetTextureManager ();

	//=============================================================================================//
	// ResourceManager取得

	ResourceManager* GetResourceManager ();

	//=============================================================================================//
	// PipelineManager取得

	PipelineManager* GetPipelineManager ();

	//=============================================================================================//
	// RootSignature取得

	ID3D12RootSignature* GetRootSignature ();

	//=============================================================================================//
	// GraphicsPipelineState取得

	ID3D12PipelineState* GetGraphicsPipelineState ();

	//=============================================================================================//
	// GraphicsPipeline生成

	void CreateGraphicsPipeline (ID3D12Device* device);

	//=============================================================================================//
	// SpritePipeline生成

	void CreateSpritePipeline(ID3D12Device* device);

	//=============================================================================================//
	// Texture生成

	uint32_t CreateTexture(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		const std::string& filePath);

	//=============================================================================================//
	// 描画前設定

	void PreDraw (
		ID3D12GraphicsCommandList* commandList,
		D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
		D3D12_INDEX_BUFFER_VIEW* indexBufferView);

	//=============================================================================================//
	// Sprite描画前設定

	void PreSpriteDraw(
		ID3D12GraphicsCommandList* commandList,
		D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
		D3D12_INDEX_BUFFER_VIEW* indexBufferView);

private:

	//=============================================================================================//
	// SRVDescriptorHeap

	DescriptorHeapManager* srvDescriptorHeap_ = nullptr;

	//=============================================================================================//
	// ShaderCompiler

	ShaderCompiler* shaderCompiler_ = nullptr;

	//=============================================================================================//
	// TextureManager

	TextureManager* textureManager_ = nullptr;

	//=============================================================================================//
	// ResourceManager

	ResourceManager* resourceManager_ = nullptr;

	//=============================================================================================//
	// PipelineManager

	PipelineManager* pipelineManager_ = nullptr;

	//=============================================================================================//
	// RootSignature

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	//=============================================================================================//
	// GraphicsPipelineState

	Microsoft::WRL::ComPtr<ID3D12PipelineState> graphicsPipelineState_ = nullptr;

};