#include "TextureManager.h"

// StringUtility
#include "StringUtility.h"

// C++
#include <cassert>

// 自作
#include "GraphicsSystem.h"

//=================================================================================================//
// Textureを読む

DirectX::ScratchImage TextureManager::LoadTexture (const std::string& filePath) {

	//=============================================================================================//
	// Textureファイルを読む

	DirectX::ScratchImage image{};

	std::wstring filePathW = StringUtility::ConvertString (filePath);

	HRESULT hr = DirectX::LoadFromWICFile (
		filePathW.c_str (),
		DirectX::WIC_FLAGS_FORCE_SRGB,
		nullptr,
		image);

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// MipMap作成

	DirectX::ScratchImage mipImages{};

	hr = DirectX::GenerateMipMaps (
		image.GetImages (),
		image.GetImageCount (),
		image.GetMetadata (),
		DirectX::TEX_FILTER_SRGB,
		0,
		mipImages);

	assert (SUCCEEDED (hr));

	return mipImages;
}

//=================================================================================================//
// Texture生成

uint32_t TextureManager::CreateTexture(DescriptorHeapManager* srvDescriptorHeap,const std::string& filePath) {

	ResourceManager* resourceManager = ResourceManager::GetInstance();

	ID3D12Device* device = GraphicsSystem::GetDevice();

	ID3D12GraphicsCommandList* commandList = GraphicsSystem::GetCommandList();

	//=============================================================================================//
	// Texture番号

	uint32_t textureIndex =	static_cast<uint32_t>(textureResources_.size());

	//=============================================================================================//
	// Texture読み込み

	DirectX::ScratchImage mipImages = LoadTexture(filePath);

	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	//=============================================================================================//
	// TextureResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = resourceManager->CreateTextureResource(device, metadata);

	//=============================================================================================//
	// Texture転送

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = resourceManager->UploadTextureData(
		textureResource,
		mipImages,
		device,
		commandList);

	//=============================================================================================//
	// SRV設定

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	srvDesc.Format = metadata.format;

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension =	D3D12_SRV_DIMENSION_TEXTURE2D;

	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//=============================================================================================//
	// SRV生成

	device->CreateShaderResourceView(
		textureResource.Get(),
		&srvDesc,
		srvDescriptorHeap->GetCPUDescriptorHandle(textureIndex));

	//=============================================================================================//
	// Resource保存

	textureResources_.push_back(textureResource);

	intermediateResources_.push_back(intermediateResource);

	return textureIndex;
}