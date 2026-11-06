#include "FrameProfiler.h"
#include <stdexcept>
#include "TextureManager.h"
#include <utility>

// C++
#include <cassert>

// 自作
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ResourceManager.h"
#include "StringUtility.h"

//=============================================================================================//
// インスタンス取得


TextureManager* TextureManager::GetInstance() {
	// 全体で共有する一つのインスタンスを返す。

	static TextureManager instance;

	return &instance;
}

//=============================================================================================//
// 初期化処理


void TextureManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {
	// 画像のSRV作成に使う描画機能と記述子管理を保持する。

	assert(dxCommon);

	assert(srvManager);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;
}

//=============================================================================================//
// 終了処理


void TextureManager::Finalize() {
	// 読込済みの画像と記述子を解放し、キャッシュを消す。

	textureDatas_.clear();

	retiredTextures_.clear();

	srvManager_ = nullptr;

	dxCommon_ = nullptr;
}

//=============================================================================================//
// Texture読み込み(読み込み済みなら番号を返す)


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

	FrameProfiler::Scope loadTime(FrameProfiler::Section::TextureLoad);

	TextureData textureData{};

	DirectX::ScratchImage mipImages = LoadTexture(filePath);

	textureData.metadata = mipImages.GetMetadata();

	textureData.hasTransparency = !mipImages.IsAlphaAllOpaque();

	textureData.resource = ResourceManager::CreateTextureResource(dxCommon_->GetDevice(), textureData.metadata);

	// GPU検証エラーから元の画像ファイルを特定できるよう、リソースへ名前を付ける。
	textureData.resource->SetName(StringUtility::ConvertString(filePath).c_str());

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

bool TextureManager::TryLoad(const std::string& filePath, std::string& error) {
	// 画像の読込を試し、失敗理由を呼出側へ返す。
    error.clear();

	if (textureDatas_.contains(filePath)) {
		return true;
	}

	// 新しい画像に必要なSRVの空きがあるか確認し、なければ読込を中止する。
	if (srvManager_->IsFull()) {
		error = "Texture descriptor limit reached.";

		return false;
	}

    try { Load(filePath); }
    catch (const std::exception& failure) { error = failure.what(); return false; }

    return true;
}

//=============================================================================================//
// Asset参照の更新

void TextureManager::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	std::vector<std::string> changedKeys;

	for (const std::pair<const std::string, TextureData>& entry : textureDatas_) {
		if (change.RemapResource(entry.first) != entry.first) {
			changedKeys.push_back(entry.first);
		}
	}

	for (const std::string& key : changedKeys) {
		const std::string destination = change.RemapResource(key);

		std::unordered_map<std::string, TextureData>::node_type node = textureDatas_.extract(key);

		if (textureDatas_.contains(destination)) {
			retiredTextures_.push_back(std::move(textureDatas_.at(destination)));

			textureDatas_.erase(destination);
		}

		node.key() = destination;

		textureDatas_.insert(std::move(node));
	}
}

//=============================================================================================//
// メタデータ取得


const DirectX::TexMetadata& TextureManager::GetMetaData(const std::string& filePath) {
	// 読み込んだ画像のサイズや形式を取得する。

	assert(textureDatas_.contains(filePath));

	return textureDatas_.at(filePath).metadata;
}

bool TextureManager::HasTransparency(const std::string& filePath) const {
	// 読込時に記録した透明度情報を確認する。
	return textureDatas_.at(filePath).hasTransparency;
}

//=============================================================================================//
// SRV番号取得


uint32_t TextureManager::GetSrvIndex(const std::string& filePath) {
	// 画像に割り当てたSRV番号を取得する。

	assert(textureDatas_.contains(filePath));

	return textureDatas_.at(filePath).srvIndex;
}

//=============================================================================================//
// GPUHandle取得


D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(const std::string& filePath) {
	// 画像を参照するGPU用SRVハンドルを取得する。

	assert(textureDatas_.contains(filePath));

	return textureDatas_.at(filePath).srvHandleGPU;
}

//=============================================================================================//
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

	if (FAILED(hr)) {
		throw std::runtime_error("Texture decoding or mip generation failed.");
	}

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

	if (FAILED(hr)) {
		throw std::runtime_error("Texture decoding or mip generation failed.");
	}

	return mipImages;
}
