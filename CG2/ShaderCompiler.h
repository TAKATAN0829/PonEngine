#pragma once

// Windows
#include <Windows.h>

// DXC
#include <dxcapi.h>

// C++
#include <string>

class ShaderCompiler {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize ();

	//=============================================================================================//
	// ShaderCompile

	IDxcBlob* Compile (
		const std::wstring& filePath,
		const wchar_t* profile);

	//=============================================================================================//
	// 終了処理

	void Finalize ();

private:

	//=============================================================================================//
	// DxcUtils

	IDxcUtils* dxcUtils_ = nullptr;

	//=============================================================================================//
	// DxcCompiler

	IDxcCompiler3* dxcCompiler_ = nullptr;

	//=============================================================================================//
	// IncludeHandler

	IDxcIncludeHandler* includeHandler_ = nullptr;
};