#pragma once

// DirectX
#include <d3d12.h>
#include <cstdint>

// WRL
#include <wrl.h>

// 自作
#include "BlendMode.h"
#include "Matrix4x4.h"

class DirectXCommon;
class ShaderCompiler;
class SrvManager;
class Mesh;

class ParticleRenderer {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// 描画前設定

	// 合成方式と画像の透明度方式を、そのDrawだけにRoot Constantsで渡す。
	void PreDraw(BlendMode blendMode, bool isBrightnessAlphaEnabled, const Matrix4x4& inverseViewProjection, float softDistance);

	// モデル描画後〜粒子描画完了まで深度を読取専用にする。コピーやGPU待機は行わない。
	void BeginPass();

	void EndPass();

	//=============================================================================================//
	// Getter

	DirectXCommon* GetDxCommon() const;

	SrvManager* GetSrvManager() const;

	Mesh* GetMesh() const;

private:

	//=============================================================================================//
	// 深度参照。GPUハンドルと寸法だけをPass内で保持する。

	uint32_t backDepthDescriptor_ = 0;
	uint32_t nullDepthDescriptor_ = 0;

	D3D12_GPU_DESCRIPTOR_HANDLE passDepth_{};

	float depthWidth_ = 1;
	float depthHeight_ = 1;

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// SrvManager

	SrvManager* srvManager_ = nullptr;

	//=============================================================================================//
	// RootSignature

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	//=============================================================================================//
	// PipelineState

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineStates_[kCountOfBlendMode];

	//=============================================================================================//
	// 全Particle共通の板ポリMesh

	Mesh* mesh_ = nullptr;
};
