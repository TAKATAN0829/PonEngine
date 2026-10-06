#pragma once

// Windows
#include <Windows.h>

// DirectX
#include <d3d12.h>

// DirectXTex
#include "DirectXTex/DirectXTex.h"

// WRL
#include <wrl.h>

// C++
#include <cstddef>

class ResourceManager {
public:

	//=============================================================================================//
	// BufferResourceを作る

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateBufferResource (
		ID3D12Device* device,
		size_t sizeInBytes,
		const char* resourceName);

	//=============================================================================================//
	// TextureResourceを作る

	static Microsoft::WRL::ComPtr<ID3D12Resource> CreateTextureResource (ID3D12Device* device, const DirectX::TexMetadata& metadata);

	//=============================================================================================//
	// TextureResourceにデータを転送

	[[nodiscard]]
	static Microsoft::WRL::ComPtr<ID3D12Resource> UploadTextureData (
		const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
		const DirectX::ScratchImage& mipImages,
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList);
};
