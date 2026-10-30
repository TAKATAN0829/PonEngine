#include <stdexcept>
#include "TextureManager.h"

// C++
#include <cassert>

// 自作
#include "DirectXCommon.h"
#include "SrvManager.h"
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

void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {

	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;
}

//=================================================================================================//
// 終了処理

void TextureManager::Finalize() {

	textureDatas_.clear();

	srvManager_ = nullptr;

	dxCommon_ = nullptr;
}

//=================================================================================================//
// Texture読み込み

uint32_t TextureManager::Load(const std::string& filePath) {

	//=============================================================================================//
	// 読み込み済みテクスチャを検索

	if (textureDatas_.contains(filePath)) {

		return textureDatas_.at(filePath).srvIndex;
	}

	//=============================================================================================//
	// テクスチャ枚数上限チェック

	assert(!srvManager_->IsFull());

	//=============================================================================================//
	// テクスチャデータを追加して書き込む

	TextureData textureData{};

	DirectX::ScratchImage mipImages = LoadTexture(filePath);

	textureData.metadata = mipImages.GetMetadata();

	textureData.resource = ResourceManager::CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);

	textureData.intermediateResource = ResourceManager::UploadTextureData(
		textureData.resource,
		mipImages,
		dxCommon_->GetDevice(),
		dxCommon_->GetCommandList());

	//=============================================================================================//
	// SRV確保

	textureData.srvIndex = srvManager_->Allocate();

	textureData.srvHandleCPU = srvManager_->GetCPUDescriptorHandle(textureData.srvIndex);

	textureData.srvHandleGPU = srvManager_->GetGPUDescriptorHandle(textureData.srvIndex);

	//=============================================================================================//
	// SRV生成

	srvManager_->CreateSRVforTexture2D(
		textureData.srvIndex,
		textureData.resource.Get(),
		textureData.metadata.format,
		UINT(textureData.metadata.mipLevels));

	const uint32_t loadedIndex = textureData.srvIndex;
    textureDatas_.emplace(filePath, std::move(textureData));
    return loadedIndex;
}

//=================================================================================================//
// メタデータ取得

const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath) {

	assert(textureDatas_.contains(filePath));

	return textureDatas_.at(filePath).metadata;
}

//=================================================================================================//
// SRV番号取得

uint32_t TextureManager::GetSrvIndex(const std::string& filePath) {

	assert(textureDatas_.contains(filePath));

	return textureDatas_.at(filePath).srvIndex;
}

//=================================================================================================//
// GPUHandle取得

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath) {

	assert(textureDatas_.contains(filePath));

	return textureDatas_.at(filePath).srvHandleGPU;
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

	if (FAILED(hr)) { throw std::runtime_error("Texture decoding or mip generation failed."); }

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

	if (FAILED(hr)) { throw std::runtime_error("Texture decoding or mip generation failed."); }

	return mipImages;
}

bool TextureManager::TryLoad(const std::string& filePath, std::string& error) {
    error.clear();
    if (textureDatas_.contains(filePath)) { return true; }
    if (srvManager_->IsFull()) { error = "Texture descriptor limit reached."; return false; }
    try { Load(filePath); }
    catch (const std::exception& failure) { error = failure.what(); return false; }
    return true;
}
