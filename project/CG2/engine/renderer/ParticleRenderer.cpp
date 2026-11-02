#include "ParticleRenderer.h"

// C++
#include <cassert>
#include <cstring>

// 自作
#include "DirectXCommon.h"
#include "PipelineManager.h"
#include "ShaderCompiler.h"
#include "Mesh.h"
#include "SrvManager.h"

//=============================================================================================//
// 初期化処理


void ParticleRenderer::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ShaderCompiler* shaderCompiler) {

	assert(dxCommon);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;
	backDepthDescriptor_ = srvManager_->Allocate();
	nullDepthDescriptor_ = srvManager_->Allocate();
	dxCommon_->InitializeDepthSampling(srvManager_,backDepthDescriptor_);
	srvManager_->CreateSRVforTexture2D(nullDepthDescriptor_,nullptr,DXGI_FORMAT_R24_UNORM_X8_TYPELESS,1);

	ID3D12Device* device = dxCommon_->GetDevice();

	PipelineManager pipelineManager;

	Microsoft::WRL::ComPtr<IDxcBlob> vertexShaderBlob = shaderCompiler->CompileShader(L"resources/shaders/Particle.VS.hlsl", L"vs_6_0");

	Microsoft::WRL::ComPtr<IDxcBlob> pixelShaderBlob = shaderCompiler->CompileShader(L"resources/shaders/Particle.PS.hlsl", L"ps_6_0");

	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};

	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//=============================================================================================//
	// Instancing用のDescriptorRange（VertexShaderのt0）

	D3D12_DESCRIPTOR_RANGE descriptorRangeForInstancing[1]{};

	descriptorRangeForInstancing[0].BaseShaderRegister = 0;
	descriptorRangeForInstancing[0].NumDescriptors = 1;
	descriptorRangeForInstancing[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRangeForInstancing[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//=============================================================================================//
	// Texture用のDescriptorRange（PixelShaderのt0）

	D3D12_DESCRIPTOR_RANGE descriptorRange[1]{};

	descriptorRange[0].BaseShaderRegister = 0;
	descriptorRange[0].NumDescriptors = 1;
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	D3D12_ROOT_PARAMETER rootParameters[5]{};

	// Material
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[0].Descriptor.ShaderRegister = 0;

	// Instancing用のStructuredBuffer
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParameters[1].DescriptorTable.pDescriptorRanges = descriptorRangeForInstancing;
	rootParameters[1].DescriptorTable.NumDescriptorRanges = _countof(descriptorRangeForInstancing);

	// Texture
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange);

	// Particle専用の表示設定。Materialの共通レイアウトや他の描画へ影響させない。
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[3].Constants.ShaderRegister = 1;
	rootParameters[3].Constants.Num32BitValues = 24;
	D3D12_DESCRIPTOR_RANGE depthRange{};
	depthRange.BaseShaderRegister = 1;
	depthRange.NumDescriptors = 1;
	depthRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	depthRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	rootParameters[4].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParameters[4].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParameters[4].DescriptorTable.NumDescriptorRanges = 1;
	rootParameters[4].DescriptorTable.pDescriptorRanges = &depthRange;

	descriptionRootSignature.pParameters = rootParameters;

	descriptionRootSignature.NumParameters = _countof(rootParameters);

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1]{};

	staticSamplers[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	// 板の縁で反対側の画素を混ぜず、透明な外側へつなぐ。
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
	staticSamplers[0].BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;
	staticSamplers[0].ShaderRegister = 0;
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	descriptionRootSignature.pStaticSamplers = staticSamplers;

	descriptionRootSignature.NumStaticSamplers = _countof(staticSamplers);

	rootSignature_ = pipelineManager.CreateRootSignature(device, descriptionRootSignature);

	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc = pipelineManager.CreateInputLayout();

	D3D12_RASTERIZER_DESC rasterizerDesc = pipelineManager.CreateRasterizerState();

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = pipelineManager.CreateDepthStencilState();

	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;

	// BlendModeの数だけPSOを生成
	for (int blendMode = 0; blendMode < kCountOfBlendMode; ++blendMode) {

		D3D12_BLEND_DESC blendDesc = pipelineManager.CreateBlendState(static_cast<BlendMode>(blendMode));
		if (blendMode != kBlendModeNone) {
			// 不透明なSceneへ半透明粒子を描いても、Scene画像自体を半透明にしない。
			blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		}
		if (blendMode == kBlendModeInvert) {
			blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		}

		pipelineStates_[blendMode] = pipelineManager.CreateGraphicsPipelineState(
			device,
			rootSignature_.Get(),
			inputLayoutDesc,
			blendDesc,
			rasterizerDesc,
			depthStencilDesc,
			vertexShaderBlob.Get(),
			pixelShaderBlob.Get());
	}

	// 全Particle共通の板ポリMesh生成
	mesh_ = new Mesh();

	mesh_->Initialize(dxCommon_, MeshType::kPlane);
}

//=============================================================================================//
// 終了処理


void ParticleRenderer::Finalize() {
	dxCommon_->FinalizeDepthSampling();
	srvManager_->Free(backDepthDescriptor_);
	srvManager_->Free(nullDepthDescriptor_);

	delete mesh_;
}

//=============================================================================================//
// 描画前設定


void ParticleRenderer::PreDraw(BlendMode blendMode, bool useBrightnessAlpha, const Matrix4x4& inverseViewProjection, float softDistance) {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	commandList->SetGraphicsRootSignature(rootSignature_.Get());

	commandList->SetPipelineState(pipelineStates_[blendMode].Get());
	// HLSLのParticleSurfaceと同じ配置。行列は行優先、全設定をDrawごとに渡す。
	struct SurfaceSettings {
		uint32_t brightnessAlpha = 0;
		uint32_t blend = 0;
		uint32_t soft = 0;
		float distance = 0;
		Matrix4x4 inverse{};
		float width = 1;
		float height = 1;
		float padding[2]{};
	};
	SurfaceSettings surfaceSettings;
	surfaceSettings.blend = static_cast<uint32_t>(blendMode);
	surfaceSettings.inverse = inverseViewProjection;
	surfaceSettings.distance = softDistance;
	surfaceSettings.width = depthWidth_;
	surfaceSettings.height = depthHeight_;
	if (useBrightnessAlpha) {
		surfaceSettings.brightnessAlpha = 1;
	}
	D3D12_GPU_DESCRIPTOR_HANDLE depthTexture = srvManager_->GetGPUDescriptorHandle(nullDepthDescriptor_);
	if (passDepth_.ptr != 0) {
		depthTexture = passDepth_;
		if (softDistance > 0) {
			surfaceSettings.soft = 1;
		}
	}
	static_assert(sizeof(SurfaceSettings) == 24*sizeof(uint32_t));
	commandList->SetGraphicsRoot32BitConstants(3,24,&surfaceSettings,0);
	commandList->SetGraphicsRootDescriptorTable(4,depthTexture);

	commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

}

void ParticleRenderer::BeginPass() {
	assert(passDepth_.ptr == 0);
	passDepth_ = dxCommon_->BeginDepthRead();
	depthWidth_ = static_cast<float>(dxCommon_->GetDepthWidth());
	depthHeight_ = static_cast<float>(dxCommon_->GetDepthHeight());
}

void ParticleRenderer::EndPass() {
	dxCommon_->EndDepthRead();
	passDepth_ = {};
}
