#pragma once

// Windows
#include <Windows.h>

// DirectX
#include <d3d12.h>

// DirectXTex
#include "../externals/DirectXTex/DirectXTex.h"

// C++
#include <string>

class TextureManager {
public:

	//=============================================================================================//
	// Textureを読む

	DirectX::ScratchImage LoadTexture (
		const std::string& filePath);

private:
};