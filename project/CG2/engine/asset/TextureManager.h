#pragma once

#include "AssetPathChange.h"

// WRL
#include <wrl.h>

// DirectX
#include <d3d12.h>

// DirectXTex
#include "DirectXTex/DirectXTex.h"

// C++
#include <string>
#include <unordered_map>
#include <cstdint>
#include <vector>

class DirectXCommon;
class SrvManager;

class TextureManager {
public:

	//=============================================================================================//
	// インスタンス取得

	static TextureManager* GetInstance();

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Texture読み込み(読み込み済みなら番号を返す)

	uint32_t Load(const std::string& filePath);

    // Projectのプレビューでは不正な画像をエラーとして返す。
    bool TryLoad(const std::string& filePath, std::string& error);

	//=============================================================================================//
	// Asset参照の更新

	void RemapAssetPaths(const AssetPathChange& change);

	//=============================================================================================//
	// メタデータ取得

	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

	// 読込時に一度だけ調べる。Particleの不透明画像を輝度マスクとして扱う判定に使う。
	bool HasTransparency(const std::string& filePath) const;

	//=============================================================================================//
	// SRV番号取得

	uint32_t GetSrvIndex(const std::string& filePath);

	//=============================================================================================//
	// GPUHandle取得

	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(const std::string& filePath);

private:

	//=============================================================================================//
	// テクスチャ1枚分のデータ

	struct TextureData {
		bool hasTransparency = false; // 少なくとも一画素のAlphaが1未満。

		DirectX::TexMetadata metadata;

		Microsoft::WRL::ComPtr<ID3D12Resource> resource;
		Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource;

		uint32_t srvIndex;

		D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU;

		D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU;
	};

	//=============================================================================================//
	// Textureファイル読み込み

	static DirectX::ScratchImage LoadTexture(const std::string& filePath);

private:

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// SrvManager

	SrvManager* srvManager_ = nullptr;

	//=============================================================================================//
	// テクスチャデータ

	std::unordered_map<std::string, TextureData> textureDatas_;

	std::vector<TextureData> retiredTextures_; // このフレームのImGui描画が参照する古い画像を保持する。
};
