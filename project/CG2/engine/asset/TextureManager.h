#pragma once

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

	//=============================================================================================//
	// メタデータ取得

	const DirectX::TexMetadata& GetMetaData(const std::string& filePath);

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
};
