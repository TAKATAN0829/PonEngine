#pragma once

// DirectX
#include <d3d12.h>

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

	void Initialize (
		ID3D12Device* device);

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
};