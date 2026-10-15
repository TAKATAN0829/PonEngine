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

class Object3dRenderer {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 描画前設定

	void PreDraw(BlendMode blendMode);

	//=============================================================================================//
	// カメラ位置設定

	void SetCameraPosition(const Vector3& position) { cameraData_->worldPosition = position; }

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

	//=============================================================================================//
	// カメラ

	Microsoft::WRL::ComPtr<ID3D12Resource> cameraResource_ = nullptr;

	CameraForGPU* cameraData_ = nullptr;
};
