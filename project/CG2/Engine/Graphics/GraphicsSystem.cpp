#include "GraphicsSystem.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void GraphicsSystem::Initialize (ID3D12Device* device) {

	SetDevice (device);

	SetGraphicsSystem (this);

	srvDescriptorHeap_ = new DescriptorHeapManager ();

	srvDescriptorHeap_->Initialize (
		device,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		128,
		true);

	shaderCompiler_ = new ShaderCompiler ();

	shaderCompiler_->Initialize ();

	textureManager_ = new TextureManager ();

	resourceManager_ = new ResourceManager ();

	pipelineManager_ = new PipelineManager ();

	object3dRenderer_ = new Object3dRenderer ();

	object3dRenderer_->Initialize (
		device,
		pipelineManager_,
		shaderCompiler_,
		srvDescriptorHeap_);

	spriteRenderer_ = new SpriteRenderer ();

	spriteRenderer_->Initialize (
		device,
		shaderCompiler_,
		srvDescriptorHeap_);

	particleRenderer_ = new ParticleRenderer ();

	particleRenderer_->Initialize (
		device,
		pipelineManager_,
		shaderCompiler_,
		srvDescriptorHeap_);
}

//=================================================================================================//
// 終了処理

void GraphicsSystem::Finalize () {

	if (object3dRenderer_ != nullptr) {

		delete object3dRenderer_;

		object3dRenderer_ = nullptr;
	}

	if (spriteRenderer_ != nullptr) {

		delete spriteRenderer_;

		spriteRenderer_ = nullptr;
	}

	if (particleRenderer_ != nullptr) {

		delete particleRenderer_;

		particleRenderer_ = nullptr;
	}

	if (srvDescriptorHeap_ != nullptr) {

		delete srvDescriptorHeap_;

		srvDescriptorHeap_ = nullptr;
	}

	if (shaderCompiler_ != nullptr) {

		shaderCompiler_->Finalize ();

		delete shaderCompiler_;

		shaderCompiler_ = nullptr;
	}

	if (textureManager_ != nullptr) {

		delete textureManager_;

		textureManager_ = nullptr;
	}

	if (resourceManager_ != nullptr) {

		delete resourceManager_;

		resourceManager_ = nullptr;
	}

	if (pipelineManager_ != nullptr) {

		delete pipelineManager_;

		pipelineManager_ = nullptr;
	}
}

//=================================================================================================//
// SRVDescriptorHeap取得

DescriptorHeapManager* GraphicsSystem::GetSRVDescriptorHeap () {

	return srvDescriptorHeap_;
}

//=================================================================================================//
// ShaderCompiler取得

ShaderCompiler* GraphicsSystem::GetShaderCompiler () {

	return shaderCompiler_;
}

//=================================================================================================//
// TextureManager取得

TextureManager* GraphicsSystem::GetTextureManager () {

	return textureManager_;
}

//=================================================================================================//
// ResourceManager取得

ResourceManager* GraphicsSystem::GetResourceManager () {

	return resourceManager_;
}

//=================================================================================================//
// PipelineManager取得

PipelineManager* GraphicsSystem::GetPipelineManager () {

	return pipelineManager_;
}

//=================================================================================================//
// Texture生成

uint32_t GraphicsSystem::CreateTexture (
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	const std::string& filePath) {

	return textureManager_->CreateTexture (srvDescriptorHeap_, filePath);
}


//=================================================================================================//
// Texture読み込み

uint32_t GraphicsSystem::LoadTexture (const std::string& name, const std::string& filePath) {

	if (textureIndexMap_.find (name) != textureIndexMap_.end ()) {

		return textureIndexMap_[name];
	}

	uint32_t textureIndex = textureManager_->CreateTexture (srvDescriptorHeap_, filePath);

	textureIndexMap_[name] = textureIndex;

	return textureIndex;
}


//=================================================================================================//
// Texture番号取得

uint32_t GraphicsSystem::GetTextureIndex (const std::string& name) {

	if (textureIndexMap_.find (name) == textureIndexMap_.end ()) {

		return 0;
	}

	return textureIndexMap_[name];
}
