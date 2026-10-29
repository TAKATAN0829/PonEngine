#include "Object3dCommon.h"

// C++
#include <cassert>

// 自作
#include "DirectXCommon.h"
#include "PipelineManager.h"
#include "ShaderCompiler.h"
#include "ResourceManager.h"

//=================================================================================================//
// 初期化処理

void Object3dCommon::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ShaderCompiler* shaderCompiler) {

	assert(dxCommon);

	assert(shaderCompiler);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;

	shaderCompiler_ = shaderCompiler;

	CreateGraphicsPipeline();

	CreateCameraResource();
}

//=================================================================================================//
// 共通描画設定

void Object3dCommon::DrawSettingCommon() {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	commandList->SetPipelineState(pipelineStates_[kBlendModeNormal].Get());

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	commandList->SetGraphicsRootConstantBufferView(4, cameraResource_->GetGPUVirtualAddress());
}

//=================================================================================================//
// BlendMode設定

void Object3dCommon::SetBlendMode(BlendMode blendMode) {

	dxCommon_->GetCommandList()->SetPipelineState(pipelineStates_[blendMode].Get());
}

//=================================================================================================//
// RootSignatureの作成

void Object3dCommon::CreateRootSignature() {

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};

	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[7]{};

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

	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].Descriptor.ShaderRegister = 2;

	rootParameters[5].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[5].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[5].Descriptor.ShaderRegister = 3;

	rootParameters[6].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[6].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[6].Descriptor.ShaderRegister = 4;

	descriptionRootSignature.pParameters = rootParameters;

	descriptionRootSignature.NumParameters = _countof(rootParameters);

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

	PipelineManager pipelineManager;

	rootSignature_ = pipelineManager.CreateRootSignature(dxCommon_->GetDevice(), descriptionRootSignature);
}

//=================================================================================================//
// GraphicsPipelineの生成

void Object3dCommon::CreateGraphicsPipeline() {

	CreateRootSignature();

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = shaderCompiler_->CompileShader(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0");

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = shaderCompiler_->CompileShader(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0");

	PipelineManager pipelineManager;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = pipelineManager.CreateInputLayout();

	D3D12_RASTERIZER_DESC rasterizerDesc = pipelineManager.CreateRasterizerState();

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = pipelineManager.CreateDepthStencilState();

	// BlendModeの数だけPSOを生成
	for (int blendMode = 0; blendMode < kCountOfBlendMode; ++blendMode) {

		D3D12_BLEND_DESC blendDesc = pipelineManager.CreateBlendState(static_cast<BlendMode>(blendMode));

		pipelineStates_[blendMode] = pipelineManager.CreateGraphicsPipelineState(
			dxCommon_->GetDevice(),
			rootSignature_.Get(),
			inputLayoutDesc,
			blendDesc,
			rasterizerDesc,
			depthStencilDesc,
			vertexShaderBlob.Get(),
			pixelShaderBlob.Get());
	}
}

//=================================================================================================//
// カメラ用リソース生成

void Object3dCommon::CreateCameraResource() {

	cameraResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(CameraForGPU), "cameraResource");

	cameraResource_->Map(0, nullptr, reinterpret_cast<void**>(&cameraData_));

	cameraData_->worldPosition = { 0.0f,0.0f,0.0f };
}
