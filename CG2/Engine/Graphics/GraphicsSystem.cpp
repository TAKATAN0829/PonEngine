#include "GraphicsSystem.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void GraphicsSystem::Initialize (ID3D12Device* device) {

	SetDevice (device);

	SetGraphicsSystem (this);

	srvDescriptorHeap_ = new DescriptorHeapManager ();

	srvDescriptorHeap_->Initialize (device,	D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,	128,true);

	shaderCompiler_ = new ShaderCompiler ();

	shaderCompiler_->Initialize ();

	textureManager_ = new TextureManager ();

	resourceManager_ = new ResourceManager ();

	pipelineManager_ = new PipelineManager ();

	CreateGraphicsPipeline (device);

	CreateDebugLinePipeline (device);
}

//=================================================================================================//
// 終了処理

void GraphicsSystem::Finalize () {

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
// RootSignature取得

ID3D12RootSignature* GraphicsSystem::GetRootSignature () {

	return rootSignature_.Get ();
}

//=================================================================================================//
// GraphicsPipelineState取得

ID3D12PipelineState* GraphicsSystem::GetGraphicsPipelineState () {

	return graphicsPipelineState_.Get ();
}

//=================================================================================================//
// GraphicsPipeline生成

void GraphicsSystem::CreateGraphicsPipeline (ID3D12Device* device) {

	IDxcBlob* vertexShaderBlob =
		shaderCompiler_->CompileShader (
			L"Object3d.VS.hlsl",
			L"vs_6_0");

	IDxcBlob* pixelShaderBlob =
		shaderCompiler_->CompileShader (
			L"Object3d.PS.hlsl",
			L"ps_6_0");

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};

	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart =
		D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[4]{};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	descriptionRootSignature.pParameters = rootParameters;

	descriptionRootSignature.NumParameters = _countof (rootParameters);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};

	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers = staticSamplers;

	descriptionRootSignature.NumStaticSamplers = _countof (staticSamplers);

	rootSignature_ =pipelineManager_->CreateRootSignature (	device,	descriptionRootSignature);

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc =	pipelineManager_->CreateInputLayout ();

	D3D12_BLEND_DESC blendDesc =pipelineManager_->CreateBlendState ();

	D3D12_RASTERIZER_DESC rasterizerDesc =	pipelineManager_->CreateRasterizerState ();

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc =	pipelineManager_->CreateDepthStencilState ();

	graphicsPipelineState_ =
		pipelineManager_->CreateGraphicsPipelineState (
			device,
			rootSignature_.Get (),
			inputLayoutDesc,
			blendDesc,
			rasterizerDesc,
			depthStencilDesc,
			vertexShaderBlob,
			pixelShaderBlob);
}

//=================================================================================================//
// SpritePipeline生成

void GraphicsSystem::CreateSpritePipeline (ID3D12Device* device) {

	pipelineManager_->CreateSpritePipeline (device,	shaderCompiler_);
}

//=================================================================================================//
// DebugLinePipeline生成

void GraphicsSystem::CreateDebugLinePipeline (ID3D12Device* device) {

	pipelineManager_->CreateDebugLinePipeline (device,	shaderCompiler_);
}

//=================================================================================================//
// Texture生成

uint32_t GraphicsSystem::CreateTexture (
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList,
	const std::string& filePath) {

	return textureManager_->CreateTexture (srvDescriptorHeap_,	filePath);
}

//=================================================================================================//
// 描画前設定

void GraphicsSystem::PreDraw (
	ID3D12GraphicsCommandList* commandList,
	D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
	D3D12_INDEX_BUFFER_VIEW* indexBufferView) {

	commandList->SetGraphicsRootSignature (rootSignature_.Get ());

	commandList->SetPipelineState (graphicsPipelineState_.Get ());

	commandList->IASetVertexBuffers (0,	1,	vertexBufferView);

	commandList->IASetIndexBuffer (indexBufferView);

	commandList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap_->GetDescriptorHeap ()};

	commandList->SetDescriptorHeaps (1,	descriptorHeaps);
}

//=================================================================================================//
// Sprite描画前設定

void GraphicsSystem::PreSpriteDraw (
	ID3D12GraphicsCommandList* commandList,
	D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
	D3D12_INDEX_BUFFER_VIEW* indexBufferView) {

	commandList->SetGraphicsRootSignature (pipelineManager_->GetSpriteRootSignature ());

	commandList->SetPipelineState (pipelineManager_->GetSpritePipelineState ());

	commandList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->IASetVertexBuffers (0,1,vertexBufferView);

	commandList->IASetIndexBuffer (indexBufferView);

	ID3D12DescriptorHeap* descriptorHeaps[] = {srvDescriptorHeap_->GetDescriptorHeap ()};

	commandList->SetDescriptorHeaps (1,	descriptorHeaps);
}