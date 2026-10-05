#include "Object3dRenderer.h"

// 自作
#include "PipelineManager.h"
#include "ShaderCompiler.h"
#include "DescriptorHeapManager.h"

//=================================================================================================//
// 初期化処理

void Object3dRenderer::Initialize(
	ID3D12Device* device,
	PipelineManager* pipelineManager,
	ShaderCompiler* shaderCompiler,
	DescriptorHeapManager* srvDescriptorHeap) {

	srvDescriptorHeap_ = srvDescriptorHeap;

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = shaderCompiler->CompileShader(L"resources/shaders/Object3d.VS.hlsl", L"vs_6_0");

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = shaderCompiler->CompileShader(L"resources/shaders/Object3d.PS.hlsl", L"ps_6_0");

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};

	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

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

	rootSignature_ = pipelineManager->CreateRootSignature(device, descriptionRootSignature);

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = pipelineManager->CreateInputLayout();

	D3D12_RASTERIZER_DESC rasterizerDesc = pipelineManager->CreateRasterizerState();

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = pipelineManager->CreateDepthStencilState();

	// BlendModeの数だけPSOを生成
	for (int blendMode = 0; blendMode < kCountOfBlendMode; ++blendMode) {

		D3D12_BLEND_DESC blendDesc = pipelineManager->CreateBlendState(static_cast<BlendMode>(blendMode));

		pipelineStates_[blendMode] = pipelineManager->CreateGraphicsPipelineState(
			device,
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
// 描画前設定

void Object3dRenderer::PreDraw(
	ID3D12GraphicsCommandList* commandList,
	D3D12_VERTEX_BUFFER_VIEW* vertexBufferView,
	D3D12_INDEX_BUFFER_VIEW* indexBufferView,
	BlendMode blendMode) {

	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	commandList->SetPipelineState(pipelineStates_[blendMode].Get());

	commandList->IASetVertexBuffers(
		0,
		1,
		vertexBufferView);

	commandList->IASetIndexBuffer(indexBufferView);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_->GetDescriptorHeap() };

	commandList->SetDescriptorHeaps(1, descriptorHeaps);
}
