#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// 自作
#include "BlendMode.h"

class DirectXCommon;
class ShaderCompiler;

class ParticleRenderer {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 描画前設定

	void PreDraw(BlendMode blendMode);

	//=============================================================================================//
	// Getter

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// RootSignature

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	//=============================================================================================//
	// PipelineState

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlendMode];
};
