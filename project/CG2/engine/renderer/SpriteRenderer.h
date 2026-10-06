#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

class DirectXCommon;
class ShaderCompiler;

class SpriteRenderer {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 描画前設定

	void PreDraw();

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

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
