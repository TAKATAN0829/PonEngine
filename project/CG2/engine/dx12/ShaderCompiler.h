#pragma once

// Windows
#include <Windows.h>

// DXC
#include <dxcapi.h>

// WRL
#include <wrl.h>

// C++
#include <string>

class ShaderCompiler {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize ();

	//=============================================================================================//
	// ShaderCompile

	Microsoft::WRL::ComPtr<IDxcBlob> CompileShader (const std::wstring& filePath, const wchar_t* profile);

	//=============================================================================================//
	// 終了処理

	void Finalize ();

private:

	//=============================================================================================//
	// DxcUtils

	Microsoft::WRL::ComPtr<IDxcUtils> dxcUtils_ = nullptr;

	//=============================================================================================//
	// DxcCompiler

	Microsoft::WRL::ComPtr<IDxcCompiler3> dxcCompiler_ = nullptr;

	//=============================================================================================//
	// IncludeHandler

	Microsoft::WRL::ComPtr<IDxcIncludeHandler> includeHandler_ = nullptr;
};