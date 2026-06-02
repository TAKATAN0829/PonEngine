#pragma once

// Windows
#include <Windows.h>

// DirectX
#include <dxcapi.h>
#include <d3d12.h>
#include <d3dcompiler.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>

// 自作
#include "ShaderCompiler.h"

class PipelineManager {
public:

	//=============================================================================================//
	// RootSignatureを作る

	Microsoft::WRL::ComPtr<ID3D12RootSignature>
		CreateRootSignature (ID3D12Device* device, D3D12_ROOT_SIGNATURE_DESC& descriptionRootSignature);

	//=============================================================================================//
	// InputLayout設定

	D3D12_INPUT_LAYOUT_DESC CreateInputLayout ();

	//=============================================================================================//
	// BlendState設定

	D3D12_BLEND_DESC CreateBlendState ();

	//=============================================================================================//
	// RasterizerState設定

	D3D12_RASTERIZER_DESC CreateRasterizerState ();

	//=============================================================================================//
	// DepthStencilState設定

	D3D12_DEPTH_STENCIL_DESC CreateDepthStencilState ();

	//=============================================================================================//
	// GraphicsPipelineStateを作る

	Microsoft::WRL::ComPtr<ID3D12PipelineState>
		CreateGraphicsPipelineState (
			ID3D12Device* device,
			ID3D12RootSignature* rootSignature,
			D3D12_INPUT_LAYOUT_DESC& inputLayoutDesc,
			D3D12_BLEND_DESC& blendDesc,
			D3D12_RASTERIZER_DESC& rasterizerDesc,
			D3D12_DEPTH_STENCIL_DESC& depthStencilDesc,
			IDxcBlob* vertexShaderBlob,
			IDxcBlob* pixelShaderBlob);

	//=============================================================================================//
	// SpritePipeline生成

	void CreateSpritePipeline(ID3D12Device* device, ShaderCompiler* shaderCompiler);

	//=============================================================================================//
	// SpriteRootSignature取得

	ID3D12RootSignature* GetSpriteRootSignature();

	//=============================================================================================//
	// SpritePipelineState取得

	ID3D12PipelineState* GetSpritePipelineState();

private:

	//=============================================================================================//
	// InputElementDesc

	D3D12_INPUT_ELEMENT_DESC inputElementDescs_[3]{};

	//=============================================================================================//
	// SpriteRootSignature

	Microsoft::WRL::ComPtr<ID3D12RootSignature>	spriteRootSignature_ = nullptr;

	//=============================================================================================//
	// SpritePipelineState

	Microsoft::WRL::ComPtr<ID3D12PipelineState>	spritePipelineState_ = nullptr;
};