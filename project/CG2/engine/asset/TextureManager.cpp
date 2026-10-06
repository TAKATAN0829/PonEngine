#include "TextureManager.h"

// C++
#include <cassert>

// 自作
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "StringUtility.h"

//=================================================================================================//
// インスタンス取得

TextureManager* TextureManager::GetInstance() {

	static TextureManager instance;

	return &instance;
}

//=================================================================================================//
// 初期化処理

void TextureManager::Initialize(DirectXCommon* dxCommon) {

	assert(dxCommon);

	dxCommon_ = dxCommon;
}

//=================================================================================================//
// 終了処理

void TextureManager::Finalize() {

	srvIndices_.clear();

	intermediateResources_.clear();

	textureResources_.clear();

	dxCommon_ = nullptr;
}

//=================================================================================================//
// Texture読み込み

uint32_t TextureManager::Load(const std::string& filePath) {

	//=============================================================================================//
	// 読み込み済みならその番号を返す

	auto it = srvIndices_.find(filePath);

	if (it != srvIndices_.end()) {
		return it->second;
	}

	ID3D12Device* device = dxCommon_->GetDevice();

	DescriptorHeapManager* srvHeap = dxCommon_->GetSrvHeap();

	//=============================================================================================//
	// Texture読み込み

	DirectX::ScratchImage mipImages = LoadTexture(filePath);

	const DirectX::TexMetadata& metadata = mipImages.GetMetadata();

	//=============================================================================================//
	// TextureResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource> textureResource = ResourceManager::CreateTextureResource(device, metadata);

	//=============================================================================================//
	// Texture転送

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = ResourceManager::UploadTextureData(
		textureResource,
		mipImages,
		device,
		dxCommon_->GetCommandList());

	//=============================================================================================//
	// SRV設定

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	srvDesc.Format = metadata.format;

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	srvDesc.Texture2D.MipLevels = UINT(metadata.mipLevels);

	//=============================================================================================//
	// SRV生成

	uint32_t srvIndex = srvHeap->Allocate();

	device->CreateShaderResourceView(
		textureResource.Get(),
		&srvDesc,
		srvHeap->GetCPUDescriptorHandle(srvIndex));

	//=============================================================================================//
	// Resource保存

	textureResources_.push_back(textureResource);

	intermediateResources_.push_back(intermediateResource);

	srvIndices_[filePath] = srvIndex;

	return srvIndex;
}

//=================================================================================================//
// Textureファイル読み込み

DirectX::ScratchImage TextureManager::LoadTexture(const std::string& filePath) {

	//=============================================================================================//
	// Textureファイルを読む

	DirectX::ScratchImage image{};

	std::wstring filePathW = StringUtility::ConvertString(filePath);

	HRESULT hr = DirectX::LoadFromWICFile(
		filePathW.c_str(),
		DirectX::WIC_FLAGS_FORCE_SRGB,
		nullptr,
		image);

	assert(SUCCEEDED(hr));

	//=============================================================================================//
	// MipMap作成

	DirectX::ScratchImage mipImages{};

	hr = DirectX::GenerateMipMaps(
		image.GetImages(),
		image.GetImageCount(),
		image.GetMetadata(),
		DirectX::TEX_FILTER_SRGB,
		0,
		mipImages);

	assert(SUCCEEDED(hr));

	return mipImages;
}
