#include "PipelineManager.h"

// LogSystem
#include "LogSystem.h"

// C++
#include <cassert>

//=================================================================================================//
// RootSignatureを作る

Microsoft::WRL::ComPtr<ID3D12RootSignature>
PipelineManager::CreateRootSignature (
	ID3D12Device* device,
	D3D12_ROOT_SIGNATURE_DESC& descriptionRootSignature) {

	//=============================================================================================//
	// RootSignatureSerialize

	ID3DBlob* signatureBlob = nullptr;

	ID3DBlob* errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature (
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		&signatureBlob,
		&errorBlob);

	if (FAILED (hr)) {

		LogSystem::Log (
			reinterpret_cast<char*>(
				errorBlob->GetBufferPointer ()));

		assert (false);
	}

	//=============================================================================================//
	// RootSignature生成

	Microsoft::WRL::ComPtr<ID3D12RootSignature>	rootSignature = nullptr;

	hr = device->CreateRootSignature (
		0,
		signatureBlob->GetBufferPointer (),
		signatureBlob->GetBufferSize (),
		IID_PPV_ARGS (&rootSignature));

	assert (SUCCEEDED (hr));

	return rootSignature;
}

//=================================================================================================//
// InputLayout設定

D3D12_INPUT_LAYOUT_DESC
PipelineManager::CreateInputLayout () {

	//=============================================================================================//
	// POSITION

	inputElementDescs_[0].SemanticName = "POSITION";
	inputElementDescs_[0].SemanticIndex = 0;
	inputElementDescs_[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;

	inputElementDescs_[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	//=============================================================================================//
	// TEXCOORD

	inputElementDescs_[1].SemanticName = "TEXCOORD";
	inputElementDescs_[1].SemanticIndex = 0;
	inputElementDescs_[1].Format = DXGI_FORMAT_R32G32_FLOAT;

	inputElementDescs_[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	//=============================================================================================//
	// NORMAL

	inputElementDescs_[2].SemanticName = "NORMAL";
	inputElementDescs_[2].SemanticIndex = 0;
	inputElementDescs_[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;

	inputElementDescs_[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	//=============================================================================================//
	// InputLayoutDesc

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};

	inputLayoutDesc.pInputElementDescs = inputElementDescs_;

	inputLayoutDesc.NumElements = _countof (inputElementDescs_);

	return inputLayoutDesc;
}

//=================================================================================================//
// BlendState設定

D3D12_BLEND_DESC
PipelineManager::CreateBlendState () {

	D3D12_BLEND_DESC blendDesc{};

	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	return blendDesc;
}

//=================================================================================================//
// RasterizerState設定

D3D12_RASTERIZER_DESC
PipelineManager::CreateRasterizerState () {

	D3D12_RASTERIZER_DESC rasterizerDesc{};

	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	return rasterizerDesc;
}

//=================================================================================================//
// DepthStencilState設定

D3D12_DEPTH_STENCIL_DESC
PipelineManager::CreateDepthStencilState () {

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};

	depthStencilDesc.DepthEnable = true;

	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;

	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	return depthStencilDesc;
}

//=================================================================================================//
// GraphicsPipelineStateを作る

Microsoft::WRL::ComPtr<ID3D12PipelineState>
PipelineManager::CreateGraphicsPipelineState (
	ID3D12Device* device,
	ID3D12RootSignature* rootSignature,
	D3D12_INPUT_LAYOUT_DESC& inputLayoutDesc,
	D3D12_BLEND_DESC& blendDesc,
	D3D12_RASTERIZER_DESC& rasterizerDesc,
	D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
	IDxcBlob* vertexShaderBlob,
	IDxcBlob* pixelShaderBlob) {

	//=============================================================================================//
	// PSO設定

	D3D12_GRAPHICS_PIPELINE_STATE_DESC
		graphicsPipelineStateDesc{};

	graphicsPipelineStateDesc.pRootSignature = rootSignature;

	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;

	graphicsPipelineStateDesc.VS = {
		vertexShaderBlob->GetBufferPointer (),
		vertexShaderBlob->GetBufferSize ()
	};

	graphicsPipelineStateDesc.PS = {
		pixelShaderBlob->GetBufferPointer (),
		pixelShaderBlob->GetBufferSize ()
	};

	graphicsPipelineStateDesc.BlendState = blendDesc;

	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;

	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;

	graphicsPipelineStateDesc.NumRenderTargets = 1;

	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;

	graphicsPipelineStateDesc.SampleMask =
		D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DSVFormat =
		DXGI_FORMAT_D24_UNORM_S8_UINT;

	//=============================================================================================//
	// PSO生成

	Microsoft::WRL::ComPtr<ID3D12PipelineState>	graphicsPipelineState = nullptr;

	HRESULT hr =
		device->CreateGraphicsPipelineState (
			&graphicsPipelineStateDesc,
			IID_PPV_ARGS (&graphicsPipelineState));

	assert (SUCCEEDED (hr));

	return graphicsPipelineState;
}