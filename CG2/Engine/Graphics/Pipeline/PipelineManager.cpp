#include "PipelineManager.h"

// LogSystem
#include "LogSystem.h"

// C++
#include <cassert>

//=================================================================================================//
// RootSignatureを作る

Microsoft::WRL::ComPtr<ID3D12RootSignature>
PipelineManager::CreateRootSignature (ID3D12Device* device, D3D12_ROOT_SIGNATURE_DESC& descriptionRootSignature) {

	//=============================================================================================//
	// RootSignatureSerialize

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr = D3D12SerializeRootSignature (
		&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1,
		signatureBlob.GetAddressOf (),
		errorBlob.GetAddressOf ());

	if (FAILED (hr)) {

		LogSystem::Log (reinterpret_cast<char*>(errorBlob->GetBufferPointer ()));

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
PipelineManager::CreateBlendState (BlendMode blendMode) {

	D3D12_BLEND_DESC blendDesc{};

	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//=============================================================================================//
	// ブレンドなし

	if (blendMode == kBlendModeNone) {

		return blendDesc;
	}

	blendDesc.RenderTarget[0].BlendEnable = true;

	//=============================================================================================//
	// α値の合成（全モード共通）

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;

	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	//=============================================================================================//
	// 色の合成

	switch (blendMode) {

	case kBlendModeNormal:

		// Src * SrcA + Dest * (1 - SrcA)
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		break;

	case kBlendModeAdd:

		// Src * SrcA + Dest * 1
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;

	case kBlendModeSubtract:

		// Dest * 1 - Src * SrcA
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_REV_SUBTRACT;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;

	case kBlendModeMultiply:

		// Src * 0 + Dest * Src
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_ZERO;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_SRC_COLOR;
		break;

	case kBlendModeScreen:

		// Src * (1 - Dest) + Dest * 1
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_INV_DEST_COLOR;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		break;

	default:

		assert (false);
		break;
	}

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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

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

	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

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


//=================================================================================================//
// SpritePipeline生成

void PipelineManager::CreateSpritePipeline (ID3D12Device* device, ShaderCompiler* shaderCompiler) {

	//=============================================================================================//
	// RootSignature作成

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//=============================================================================================//
	// DescriptorRange作成

	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};

	descriptorRange[0].BaseShaderRegister = 0;

	descriptorRange[0].NumDescriptors = 1;

	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;

	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//=============================================================================================//
	// RootParameter作成

	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	// Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// WVP
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

	descriptionRootSignature.NumParameters = _countof (rootParameters);

	//=============================================================================================//
	// Sampler作成

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};

	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;

	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;

	staticSamplers[0].ShaderRegister = 0;

	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers = staticSamplers;

	descriptionRootSignature.NumStaticSamplers = 1;

	//=============================================================================================//
	// RootSignatureシリアライズ

	Microsoft::WRL::ComPtr<ID3DBlob> signatureBlob = nullptr;

	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob = nullptr;

	HRESULT hr =
		D3D12SerializeRootSignature (
			&descriptionRootSignature,
			D3D_ROOT_SIGNATURE_VERSION_1,
			&signatureBlob,
			&errorBlob);

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// RootSignature生成

	hr = device->CreateRootSignature (
		0,
		signatureBlob->GetBufferPointer (),
		signatureBlob->GetBufferSize (),
		IID_PPV_ARGS (&spriteRootSignature_));

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// InputLayout設定

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};

	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};

	inputLayoutDesc.pInputElementDescs = inputElementDescs;

	inputLayoutDesc.NumElements = _countof (inputElementDescs);

	//=============================================================================================//
	// BlendState設定

	D3D12_BLEND_DESC blendDesc{};

	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	blendDesc.RenderTarget[0].BlendEnable = true;

	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;

	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;

	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;

	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;

	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;

	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	//=============================================================================================//
	// RasterizerState設定

	D3D12_RASTERIZER_DESC rasterizerDesc{};

	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;

	//=============================================================================================//
	// Shaderコンパイル

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob =
		shaderCompiler->CompileShader (
			L"Object3d.VS.hlsl",
			L"vs_6_0");

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob =
		shaderCompiler->CompileShader (
			L"Object3d.PS.hlsl",
			L"ps_6_0");

	//=============================================================================================//
	// PSO作成

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};

	graphicsPipelineStateDesc.pRootSignature = spriteRootSignature_.Get ();

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

	graphicsPipelineStateDesc.NumRenderTargets = 1;

	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	graphicsPipelineStateDesc.SampleDesc.Count = 1;

	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	graphicsPipelineStateDesc.DepthStencilState.DepthEnable = false;

	graphicsPipelineStateDesc.DepthStencilState.StencilEnable = false;

	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	//=============================================================================================//
	// PipelineState生成

	hr = device->CreateGraphicsPipelineState (
		&graphicsPipelineStateDesc,
		IID_PPV_ARGS (&spritePipelineState_));

	assert (SUCCEEDED (hr));
}


//=================================================================================================//
// SpriteRootSignature取得

ID3D12RootSignature* PipelineManager::GetSpriteRootSignature () {

	return spriteRootSignature_.Get ();
}


//=================================================================================================//
// SpritePipelineState取得

ID3D12PipelineState* PipelineManager::GetSpritePipelineState () {

	return spritePipelineState_.Get ();
}