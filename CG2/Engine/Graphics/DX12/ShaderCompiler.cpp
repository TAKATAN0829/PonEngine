#include "ShaderCompiler.h"

// C++
#include <cassert>
#include <format>

// Library
#pragma comment(lib,"dxcompiler.lib")

// LogSystem
#include "LogSystem.h"
#include "StringUtility.h"

//=============================================================================================//
// 初期化処理

void ShaderCompiler::Initialize () {

	HRESULT hr;

	//=============================================================================================//
	// DxcUtils生成

	hr = DxcCreateInstance (CLSID_DxcUtils,IID_PPV_ARGS (&dxcUtils_));

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// DxcCompiler生成

	hr = DxcCreateInstance (CLSID_DxcCompiler,	IID_PPV_ARGS (&dxcCompiler_));

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// IncludeHandler生成

	hr = dxcUtils_->CreateDefaultIncludeHandler (&includeHandler_);

	assert (SUCCEEDED (hr));
}

//=============================================================================================//
// ShaderCompile

Microsoft::WRL::ComPtr<IDxcBlob> ShaderCompiler::CompileShader (const std::wstring& filePath, const wchar_t* profile) {

	//=============================================================================================//
	// Compile開始ログ

	LogSystem::Log (StringUtility::ConvertString (std::format (
		L"Begin CompileShader, path:{}, profile:{}\n",
		filePath,
		profile)));

	//=============================================================================================//
	// Shaderファイル読み込み

	Microsoft::WRL::ComPtr<IDxcBlobEncoding> shaderSource = nullptr;

	HRESULT hr = dxcUtils_->LoadFile (
		filePath.c_str (),
		nullptr,
		&shaderSource);

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// ShaderSource設定

	DxcBuffer shaderSourceBuffer{};

	shaderSourceBuffer.Ptr =shaderSource->GetBufferPointer ();

	shaderSourceBuffer.Size =shaderSource->GetBufferSize ();

	shaderSourceBuffer.Encoding =DXC_CP_UTF8;

	//=============================================================================================//
	// Compile設定

	LPCWSTR arguments[] = {
		filePath.c_str (),
		L"-E",L"main",
		L"-T",profile,
		L"-Zi",L"-Qembed_debug",
		L"-Od",
		L"-Zpr",
	};

	//=============================================================================================//
	// ShaderCompile

	Microsoft::WRL::ComPtr<IDxcResult> shaderResult = nullptr;

	hr = dxcCompiler_->Compile (
		&shaderSourceBuffer,
		arguments,
		_countof (arguments),
		includeHandler_.Get (),
		IID_PPV_ARGS (&shaderResult));

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// ShaderError確認

	Microsoft::WRL::ComPtr<IDxcBlobUtf8> shaderError = nullptr;

	shaderResult->GetOutput (
		DXC_OUT_ERRORS,
		IID_PPV_ARGS (&shaderError),
		nullptr);

	if (shaderError != nullptr &&
		shaderError->GetStringLength () != 0) {

		LogSystem::Log (shaderError->GetStringPointer ());

		assert (false);
	}

	//=============================================================================================//
	// ShaderBlob取得

	Microsoft::WRL::ComPtr<IDxcBlob> shaderBlob = nullptr;

	hr = shaderResult->GetOutput (
		DXC_OUT_OBJECT,
		IID_PPV_ARGS (&shaderBlob),
		nullptr);

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// Compile成功ログ

	LogSystem::Log (StringUtility::ConvertString (std::format (
		L"Compile Succeeded, path:{}, profile:{}\n",
		filePath,
		profile)));

	return shaderBlob;
}

//=============================================================================================//
// 終了処理

void ShaderCompiler::Finalize () {

	includeHandler_.Reset ();

	dxcCompiler_.Reset ();

	dxcUtils_.Reset ();
}