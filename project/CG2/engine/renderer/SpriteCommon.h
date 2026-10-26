#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

class DirectXCommon;
class ShaderCompiler;
class SrvManager;
class Mesh;

//=================================================================================================//
// スプライト共通部

class SpriteCommon {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// 共通描画設定

	void DrawSettingCommon();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Getter

	DirectXCommon* GetDxCommon() const { return dxCommon_; }

	SrvManager* GetSrvManager() const { return srvManager_; }

	Mesh* GetMesh() const { return mesh_; }

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
	// SrvManager

	SrvManager* srvManager_ = nullptr;

	//=============================================================================================//
	// ShaderCompiler

	ShaderCompiler* shaderCompiler_ = nullptr;

	//=============================================================================================//
	// RootSignature

	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;

	//=============================================================================================//
	// PipelineState

	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_ = nullptr;

	//=============================================================================================//
	// 全Sprite共通の単位矩形Mesh

	Mesh* mesh_ = nullptr;
};
