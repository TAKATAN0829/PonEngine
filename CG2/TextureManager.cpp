#include "TextureManager.h"

// LogSystem
#include "LogSystem.h"

// C++
#include <cassert>

//=================================================================================================//
// Textureを読む

DirectX::ScratchImage TextureManager::LoadTexture (
	const std::string& filePath) {

	//=============================================================================================//
	// Textureファイルを読む

	DirectX::ScratchImage image{};

	std::wstring filePathW =
		LogSystem::ConvertString (filePath);

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