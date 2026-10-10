#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

class DirectXCommon;
class ShaderCompiler;

//=================================================================================================//
// スプライト共通部

class SpriteCommon {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 共通描画設定

	void DrawSettingCommon();

	//=============================================================================================//
	// Getter

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

private:

	//=============================================================================================//
	// RootSignatureの作成

	void CreateRootSignature();

	//=============================================================================================//
	// GraphicsPipelineの生成

	void CreateGraphicsPipeline();

private:

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// ShaderCompiler

	ShaderCompiler* shaderCompiler_ = nullptr;

	//=============================================================================================//
	// RootSignature

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	//=============================================================================================//
	// PipelineState

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;
};
