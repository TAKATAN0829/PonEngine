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
#include "Object3dRenderer.h"
#include "SpriteRenderer.h"
#include "ParticleRenderer.h"

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
	// Renderer取得

	Object3dRenderer* GetObject3dRenderer () { return object3dRenderer_; }

	SpriteRenderer* GetSpriteRenderer () { return spriteRenderer_; }

	ParticleRenderer* GetParticleRenderer () { return particleRenderer_; }

	//=============================================================================================//
	// Texture生成

	uint32_t CreateTexture (
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		const std::string& filePath);

	//=============================================================================================//
	// device

	static void SetDevice (ID3D12Device* device) {
		device_ = device;
	}

	static ID3D12Device* GetDevice () { return device_; }

	//=============================================================================================//
	// commandList

	static void SetCommandList (ID3D12GraphicsCommandList* commandList) {
		commandList_ = commandList;
	}

	static ID3D12GraphicsCommandList* GetCommandList () { return commandList_; }

	//=============================================================================================//
	// GraphicsSystem

	static void SetGraphicsSystem (GraphicsSystem* graphicsSystem) {
		graphicsSystem_ = graphicsSystem;
	}

	static GraphicsSystem* GetGraphicsSystem () { return graphicsSystem_; }

	//=============================================================================================//
	// Texture読み込み

	uint32_t LoadTexture (const std::string& name, const std::string& filePath);

	//=============================================================================================//
	// Texture番号取得

	uint32_t GetTextureIndex (const std::string& name);

private:

	DescriptorHeapManager* srvDescriptorHeap_ = nullptr;

	ShaderCompiler* shaderCompiler_ = nullptr;

	TextureManager* textureManager_ = nullptr;

	ResourceManager* resourceManager_ = nullptr;

	PipelineManager* pipelineManager_ = nullptr;

	Object3dRenderer* object3dRenderer_ = nullptr;

	SpriteRenderer* spriteRenderer_ = nullptr;

	ParticleRenderer* particleRenderer_ = nullptr;

	inline static ID3D12Device* device_ = nullptr;

	inline static ID3D12GraphicsCommandList* commandList_ = nullptr;

	inline static GraphicsSystem* graphicsSystem_ = nullptr;

	std::unordered_map<std::string, uint32_t> textureIndexMap_;
};
