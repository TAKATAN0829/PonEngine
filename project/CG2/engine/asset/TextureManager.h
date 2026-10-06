#pragma once

// WRL
#include <wrl.h>

// DirectX
#include <d3d12.h>

// DirectXTex
#include "DirectXTex/DirectXTex.h"

// C++
#include <string>
#include <vector>
#include <unordered_map>
#include <cstdint>

class DirectXCommon;

class TextureManager {
public:

	//=============================================================================================//
	// インスタンス取得

	static TextureManager* GetInstance();

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon);

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Texture読み込み(読み込み済みなら番号を返す)

	uint32_t Load(const std::string& filePath);

private:

	//=============================================================================================//
	// Textureファイル読み込み

	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

private:

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// TextureResource

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textureResources_;

	//=============================================================================================//
	// IntermediateResource

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources_;

	//=============================================================================================//
	// FilePathとSRV番号の対応

	std::unordered_map<std::string, uint32_t> srvIndices_;
};
