#include "GraphicsSystem.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void GraphicsSystem::Initialize (
	ID3D12Device* device) {

	//=============================================================================================//
	// SRVDescriptorHeap生成

	srvDescriptorHeap_ = new DescriptorHeapManager ();

	srvDescriptorHeap_->Initialize (
		device,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		128,
		true);

	//=============================================================================================//
	// ShaderCompiler生成

	shaderCompiler_ = new ShaderCompiler ();

	shaderCompiler_->Initialize ();

	//=============================================================================================//
	// TextureManager生成

	textureManager_ = new TextureManager ();

	//=============================================================================================//
	// ResourceManager生成

	resourceManager_ = new ResourceManager ();

	//=============================================================================================//
	// PipelineManager生成

	pipelineManager_ = new PipelineManager ();
}

//=================================================================================================//
// 終了処理

void GraphicsSystem::Finalize () {

	//=============================================================================================//
	// SRVDescriptorHeap解放

	if (srvDescriptorHeap_ != nullptr) {

		delete srvDescriptorHeap_;

		srvDescriptorHeap_ = nullptr;
	}

	//=============================================================================================//
	// ShaderCompiler解放

	if (shaderCompiler_ != nullptr) {

		shaderCompiler_->Finalize ();

		delete shaderCompiler_;

		shaderCompiler_ = nullptr;
	}

	//=============================================================================================//
	// TextureManager解放

	if (textureManager_ != nullptr) {

		delete textureManager_;

		textureManager_ = nullptr;
	}

	//=============================================================================================//
	// ResourceManager解放

	if (resourceManager_ != nullptr) {

		delete resourceManager_;

		resourceManager_ = nullptr;
	}

	//=============================================================================================//
	// PipelineManager解放

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