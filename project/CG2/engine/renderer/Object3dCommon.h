#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// 自作
#include "BlendMode.h"
#include "EngineStructs.h"

class DirectXCommon;
class ShaderCompiler;

class Object3dCommon {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 共通描画設定

	void DrawSettingCommon();

	//=============================================================================================//
	// BlendMode設定

	void SetBlendMode(BlendMode blendMode);

	//=============================================================================================//
	// カメラ位置設定

	void SetCameraPosition(const Vector3& position) { cameraData_->worldPosition = position; }

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

	//=============================================================================================//
	// カメラ用リソース生成

	void CreateCameraResource();

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

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlendMode];

	//=============================================================================================//
	// カメラ

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_ = nullptr;

	CameraForGPU* cameraData_ = nullptr;
};
