#include "GraphicsSystem.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void GraphicsSystem::Initialize(ID3D12Device* device) {

	//=============================================================================================//
	// SRVDescriptorHeap生成

	srvDescriptorHeap_ = new DescriptorHeapManager();

	srvDescriptorHeap_->Initialize(
		device,
		D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
		128,
		true);

	//=============================================================================================//
	// ShaderCompiler生成

	shaderCompiler_ = new ShaderCompiler();

	shaderCompiler_->Initialize();

	//=============================================================================================//
	// TextureManager生成

	textureManager_ = new TextureManager();

	//=============================================================================================//
	// ResourceManager生成

	resourceManager_ = new ResourceManager();

	//=============================================================================================//
	// PipelineManager生成

	pipelineManager_ = new PipelineManager();
}

//=================================================================================================//
// 終了処理

void GraphicsSystem::Finalize() {

	//=============================================================================================//
	// SRVDescriptorHeap解放

	if (srvDescriptorHeap_ != nullptr) {

		delete srvDescriptorHeap_;

		srvDescriptorHeap_ = nullptr;
	}

	//=============================================================================================//
	// ShaderCompiler解放

	if (shaderCompiler_ != nullptr) {

		shaderCompiler_->Finalize();

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

DescriptorHeapManager* GraphicsSystem::GetSRVDescriptorHeap() {

	return srvDescriptorHeap_;
}

//=================================================================================================//
// ShaderCompiler取得

ShaderCompiler* GraphicsSystem::GetShaderCompiler() {

	return shaderCompiler_;
}

//=================================================================================================//
// TextureManager取得

TextureManager* GraphicsSystem::GetTextureManager() {

	return textureManager_;
}

//=================================================================================================//
// ResourceManager取得

ResourceManager* GraphicsSystem::GetInstance() {

	return resourceManager_;
}

//=================================================================================================//
// PipelineManager取得

PipelineManager* GraphicsSystem::GetPipelineManager() {

	return pipelineManager_;
}

//=================================================================================================//
// RootSignature取得

ID3D12RootSignature* GraphicsSystem::GetRootSignature() {

	return rootSignature_.Get();
}

//=================================================================================================//
// GraphicsPipelineState取得

ID3D12PipelineState* GraphicsSystem::GetGraphicsPipelineState() {

	return graphicsPipelineState_.Get();
}

//=================================================================================================//
// GraphicsPipeline生成

void GraphicsSystem::CreateGraphicsPipeline(ID3D12Device* device) {

	//=============================================================================================//
	// ShaderCompile

	IDxcBlob* vertexShaderBlob =
		shaderCompiler_->CompileShader(
			L"Object3d.VS.hlsl",
			L"vs_6_0");

	IDxcBlob* pixelShaderBlob =
		shaderCompiler_->CompileShader(
			L"Object3d.PS.hlsl",
			L"ps_6_0");

	//=============================================================================================//
	// RootSignature作成

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//=============================================================================================//
	// DescriptorRange

	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};

	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//=============================================================================================//
	// RootParameter

	D3D12_ROOT_PARAMETER rootParameters[4]{};

	// Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// TransformationMatrix
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].Descriptor.ShaderRegister = 0;

	// Texture
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = 1;

	// DirectionalLight
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Descriptor.ShaderRegister = 1;

	descriptionRootSignature.pParameters = rootParameters;

	descriptionRootSignature.NumParameters = _countof(rootParameters);

	//=============================================================================================//
	// Sampler設定

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

	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	//=============================================================================================//
	// RootSignature生成

	rootSignature_ =
		pipelineManager_->CreateRootSignature(device, descriptionRootSignature);

	//=============================================================================================//
	// InputLayout設定

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = pipelineManager_->CreateInputLayout();

	//=============================================================================================//
	// BlendState設定

	D3D12_BLEND_DESC blendDesc = pipelineManager_->CreateBlendState();

	//=============================================================================================//
	// RasterizerState設定

	D3D12_RASTERIZER_DESC rasterizerDesc = pipelineManager_->CreateRasterizerState();

	//=============================================================================================//
	// DepthStencilState設定

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = pipelineManager_->CreateDepthStencilState();

	//=============================================================================================//
	// PSO生成

	graphicsPipelineState_ =
		pipelineManager_->CreateGraphicsPipelineState(
			device,
			rootSignature_.Get(),
			inputLayoutDesc,
			blendDesc,
			rasterizerDesc,
			depthStencilDesc,
			vertexShaderBlob,
			pixelShaderBlob);
}

//=================================================================================================//
// SpritePipeline生成

void GraphicsSystem::CreateSpritePipeline(ID3D12Device* device) {

	pipelineManager_->CreateSpritePipeline(device, shaderCompiler_);
}

//=================================================================================================//
// Texture生成

uint32_t GraphicsSystem::CreateTexture(ID3D12Device* device, ID3D12GraphicsCommandList* commandList, const std::string& filePath) {

	return textureManager_->CreateTexture(
		device,
		commandList,
		resourceManager_,
		srvDescriptorHeap_,
		filePath);
}

//=================================================================================================//
// 描画前設定

void GraphicsSystem::PreDraw(
	ID3D12GraphicsCommandList* commandList,
	D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
	D3D12_INDEX_BUFFER_VIEW* indexBufferView) {

	//=============================================================================================//
	// RootSignature設定

	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	//=============================================================================================//
	// PipelineState設定

	commandList->SetPipelineState(graphicsPipelineState_.Get());

	//=============================================================================================//
	// VertexBuffer設定

	commandList->IASetVertexBuffers(0, 1, vertexBufferView);

	//=============================================================================================//
	// IndexBuffer設定

	commandList->IASetIndexBuffer(indexBufferView);

	//=============================================================================================//
	// PrimitiveTopology設定

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//=============================================================================================//
	// DescriptorHeap設定

	ID3D12DescriptorHeap* descriptorHeaps[] = {
		srvDescriptorHeap_->GetDescriptorHeap()
	};

	commandList->SetDescriptorHeaps(1, descriptorHeaps);
}

//=================================================================================================//
// Sprite描画前設定

void GraphicsSystem::PreSpriteDraw(
	ID3D12GraphicsCommandList* commandList,
	D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
	D3D12_INDEX_BUFFER_VIEW* indexBufferView) {

	//=============================================================================================//
	// RootSignature設定

	commandList->SetGraphicsRootSignature(pipelineManager_->GetSpriteRootSignature());

	//=============================================================================================//
	// PipelineState設定

	commandList->SetPipelineState(pipelineManager_->GetSpritePipelineState());

	//=============================================================================================//
	// PrimitiveTopology設定

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//=============================================================================================//
	// VertexBuffer設定

	commandList->IASetVertexBuffers(
		0,
		1,
		vertexBufferView);

	//=============================================================================================//
	// IndexBuffer設定

	commandList->IASetIndexBuffer(indexBufferView);

	//=============================================================================================//
	// DescriptorHeap設定

	ID3D12DescriptorHeap* descriptorHeaps[] = {
		srvDescriptorHeap_->GetDescriptorHeap()
	};

	commandList->SetDescriptorHeaps(
		1,
		descriptorHeaps);
}

