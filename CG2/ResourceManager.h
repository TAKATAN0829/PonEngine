#pragma once

// Windows
#include <Windows.h>

// DirectX
#include <d3d12.h>

// DirectXTex
#include "../externals/DirectXTex/DirectXTex.h"

// WRL
#include <wrl.h>

// C++
#include <cstddef>

class ResourceManager {
public:

	//=============================================================================================//
	// BufferResourceを作る

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource (
		ID3D12Device* device,
		size_t sizeInBytes,
		const char* resourceName);

	//=============================================================================================//
	// TextureResourceを作る

	Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource (
		ID3D12Device* device,
		const DirectX::TexMetadata& metadata);

	//=============================================================================================//
	// TextureResourceにデータを転送

	[[nodiscard]]
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData (
		const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages,
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList);

	//=============================================================================================//
	// 自身のインスタンス

	static void SetInstance(ResourceManager* resourceManager) {
		resourceManager = resourceManager_;
	}

	static ResourceManager* GetInstance() {
		return resourceManager_;
	}

private:
	inline static ResourceManager* resourceManager_ = nullptr;
};