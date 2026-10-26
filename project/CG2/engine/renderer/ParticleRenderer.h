#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// 自作
#include "BlendMode.h"

class DirectXCommon;
class ShaderCompiler;
class Mesh;

class ParticleRenderer {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// 描画前設定

	void PreDraw(BlendMode blendMode);

	//=============================================================================================//
	// Getter

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

	Mesh* GetMesh() const { return mesh_; }

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
	// 全Particle共通の板ポリMesh

	Mesh* mesh_ = nullptr;
};
