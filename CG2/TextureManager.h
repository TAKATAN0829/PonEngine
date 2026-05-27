#pragma once

// Windows
#include <Windows.h>

// WRL
#include <wrl.h>

// DirectX
#include <d3d12.h>

// DirectXTex
#include "../externals/DirectXTex/DirectXTex.h"

// C++
#include <string>
#include <vector>

// 自作
#include "ResourceManager.h"
#include "DescriptorHeapManager.h"

class TextureManager {
public:

	//=============================================================================================//
	// Textureを読む

	DirectX::ScratchImage LoadTexture (const std::string& filePath);

	//=============================================================================================//
	// Texture生成

	uint32_t CreateTexture(
		ID3D12Device* device,
		ID3D12GraphicsCommandList* commandList,
		ResourceManager* resourceManager,
		DescriptorHeapManager* srvDescriptorHeap,
		const std::string& filePath);

private:
	//=============================================================================================//
	// TextureResource

	std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> textureResources_;

	//=============================================================================================//
	// IntermediateResource

	std::vector < Microsoft::WRL::ComPtr<ID3D12Resource>> intermediateResources_;

};