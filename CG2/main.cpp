#pragma warning(push)
// C4023の警告をみなかったことにする
#pragma warning(disable:4023)

#include <Windows.h>
#include <cstdint>
#include <string>
#include <format>
#include <filesystem>
// ファイルに書いたり読んだりするライブラリ
#include <fstream>
// 時間を扱うライブラリ
#include <chrono>
#include <d3d12.h>
#pragma comment(lib,"d3d12.lib")
#include <dxgi1_6.h>
#pragma comment(lib,"dxgi.lib")
#include <cassert>
// Debug用のあれやこれやを使えるようにする
#include <dbgHelp.h>
#pragma comment(lib,"Dbghelp.lib")
#include <strsafe.h>
#include <dxgidebug.h>
#pragma comment(lib,"dxguid.lib")
#include <dxcapi.h>
#pragma comment(lib,"dxcompiler.lib")
#include "../externals/DirectXTex/DirectXTex.h"
#include "../externals/DirectXTex/d3dx12.h"
#include <vector>
#include <numbers>

// ImGui
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "../externals/imgui/imgui_impl_dx12.h"
#include "../externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

#pragma warning(pop)


#include "MT3.h"

// Transform情報を作る
struct Transform {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

// 頂点データの拡張
struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

// マテリアルの拡張
struct Material {
	Vector4 color;
	int32_t enableLighting;
};

// TransformationMatrixを拡張する
struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
};

// 平行光源
struct DirectionalLight {
	Vector4 color; // ライトの色
	Vector3 direction; // ライトの向き
	float intensity; // 輝度
};

const uint32_t kSubdivision = 16;
const uint32_t kVertexCount = (kSubdivision + 1) * (kSubdivision + 1);
const uint32_t kIndexCount = kSubdivision * kSubdivision * 6;

// ウィンドウプロシージャ
LRESULT CALLBACK WindowProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler (hwnd, msg, wParam, lParam)) {
		return true;
	}
#endif
	// メッセージに応じてゲーム固有の処理を行う
	switch (msg) {
		// ウィンドウが破棄された
	case WM_DESTROY:
	// OSに対して、アプリの終了を伝える
	PostQuitMessage (0);
	return 0;
	}

	// 標準のメッセージ処理を行う
	return DefWindowProc (hwnd, msg, wParam, lParam);
}

// 出力ウィンドウに文字を出す
void Log (const std::string& message) {
	OutputDebugStringA (message.c_str ());
}

void Log (std::ostream& os, const std::string& message) {
	os << message << std::endl;
	OutputDebugStringA (message.c_str ());
}

// string->wstring
std::wstring ConvertString (const std::string& str) {
	if (str.empty ()) {
		return std::wstring ();
	}

	auto sizeNeeded = MultiByteToWideChar (CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size ()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring ();
	}
	std::wstring result (sizeNeeded, 0);
	MultiByteToWideChar (CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size ()), &result[0], sizeNeeded);
	return result;
}

// wstring->string
std::string ConvertString (const std::wstring& str) {
	if (str.empty ()) {
		return std::string ();
	}

	auto sizeNeeded = WideCharToMultiByte (CP_UTF8, 0, str.data (), static_cast<int>(str.size ()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string ();
	}
	std::string result (sizeNeeded, 0);
	WideCharToMultiByte (CP_UTF8, 0, str.data (), static_cast<int>(str.size ()), result.data (), sizeNeeded, NULL, NULL);
	return result;
}

void Log (const std::wstring& message) {
	// wstring->stringへ変換して
	// stringの方のLog関数を呼ぶ
	Log (ConvertString (message));
}

// SEHに対して関数を作成し、登録する
static LONG WINAPI ExportDump (EXCEPTION_POINTERS* exception) {
	// 時刻を取得して、時刻を名前に入れたファイルを作成。Dumpディレクトリ以下に出力
	SYSTEMTIME time;
	GetLocalTime (&time);
	wchar_t filePath[MAX_PATH] = { 0 };
	CreateDirectory (L"./Dumps", nullptr);
	StringCchPrintfW (filePath, MAX_PATH, L"./Dumps/%04d-%02d%02d-%02d%02d.dmp", time.wYear, time.wMonth, time.wDay, time.wHour, time.wMinute);
	HANDLE dumpFileHandle = CreateFile (filePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
	// processId（このexeのId）とクラッシュ（例外）の発生したthreadIdを取得
	DWORD processId = GetCurrentProcessId ();
	DWORD threadId = GetCurrentThreadId ();
	// 設定情報を入力
	MINIDUMP_EXCEPTION_INFORMATION minidumpInfomation{ 0 };
	minidumpInfomation.ThreadId = threadId;
	minidumpInfomation.ExceptionPointers = exception;
	minidumpInfomation.ClientPointers = TRUE;
	// Dumpを出力。MiniDumpNormalは最低限の情報を出力するフラグ
	MiniDumpWriteDump (GetCurrentProcess (), processId, dumpFileHandle, MiniDumpNormal, &minidumpInfomation, nullptr, nullptr);
	// ほかに関連付けられているSEH例外ハンドラがあれば実行。通常はプロセスを終了する
	return EXCEPTION_EXECUTE_HANDLER;
}

//=================================================================================================//
// CompileShader関数                                               <-CG2_02_00
IDxcBlob* CompileShader (
	// CompilerするShaderファイルへのパス
	const std::wstring& filePath,
	// Compilerに使用するProfile
	const wchar_t* profile,
	// 初期化で生成したものを3つ
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxCompiler,
	IDxcIncludeHandler* includeHandler) {

	//===========================================//
	// 1.hlslファイルを読む

	//これからシェーダーをコンパイルする旨をログに出す
	Log (ConvertString (std::format (L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));
	// hlslファイルを読む
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile (filePath.c_str (), nullptr, &shaderSource);
	// 読めなかったら止める
	assert (SUCCEEDED (hr));
	// 読み込んファイルの内容を設定する
	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer ();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize ();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8; // UTF8の文字コードであることを通知
	//===========================================//

	//===========================================//
	// 2.Compileする

	LPCWSTR arguments[] = {
		filePath.c_str (), // コンパイル対象のhlslファイル名
		L"-E",L"main", // エントリーポイントの指定。基本的にmain以外にしない
		L"-T",profile, // ShaderProfileの設定
		L"-Zi",L"-Qembed_debug",	// デバッグ用の情報を埋め込む
		L"-Od",		// 最適化を外しておく
		L"-Zpr",	// メモリレイアウトを行優先
	};
	// 実際にShaderをコンパイルする
	IDxcResult* shaderResult = nullptr;
	hr = dxCompiler->Compile (
		&shaderSourceBuffer,	// 読み込んだファイル
		arguments,				// コンパイルオプション
		_countof (arguments),	// コンパイルオプションの数
		includeHandler,			// includeが含まれた諸々
		IID_PPV_ARGS (&shaderResult)	// コンパイル結果
	);
	// コンパイルエラーではなくdxcが起動できないなど致命的な状況
	assert (SUCCEEDED (hr));
	//===========================================//

	//===========================================//
	// 3.警告・エラーがでていないか確認する

	//警告・エラーが出てたらログに出して止める
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput (DXC_OUT_ERRORS, IID_PPV_ARGS (&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength () != 0) {
		Log (shaderError->GetStringPointer ());
		// 警告・エラーダメゼッタイ
		assert (false);
	}
	//===========================================//

	//===========================================//
	// 4.Compile結果を受け取って返す

	// コンパイル結果から実行時のバイナリ部分を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput (DXC_OUT_OBJECT, IID_PPV_ARGS (&shaderBlob), nullptr);
	assert (SUCCEEDED (hr));
	// 成功したログを出す
	Log (ConvertString (std::format (L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));
	// もう使わないリソースを開放
	shaderSource->Release ();
	shaderResult->Release ();
	// 実行時のバイナリを返却
	return shaderBlob;
	//===========================================//	
}


//=================================================================================================//
// BufferResourceを作る関数

ID3D12Resource* CreateBufferResource (ID3D12Device* device, size_t sizeInBytes, const char* resourceName) {

	// 頂点リソース用のヒープの設定
	D3D12_HEAP_PROPERTIES uploadHeapProperties{};
	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソースの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 実際にリソースを作る
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource (
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&resource)
	);
	assert (SUCCEEDED (hr));
	Log (std::string (resourceName) + " is Generated!!\n");

	return resource;
}
//=================================================================================================//


//=================================================================================================//
// DescriptorHeapの作成関数

ID3D12DescriptorHeap* CreateDescriptorHeap (ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible, const char* descriptorHeapName) {
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};
	descriptorHeapDesc.Type = heapType;
	descriptorHeapDesc.NumDescriptors = numDescriptors;
	descriptorHeapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	HRESULT hr = device->CreateDescriptorHeap (&descriptorHeapDesc, IID_PPV_ARGS (&descriptorHeap));
	assert (SUCCEEDED (hr));
	Log (std::string (descriptorHeapName) + " is Generated!!\n");
	return descriptorHeap;
}
//=================================================================================================//


//=================================================================================================//
// Textureを読んで使えるようにする

//===============================================//
// 1.Textureデータを読む

DirectX::ScratchImage LoadTexture (const std::string& filePath) {
	// テクスチャファイルを読んでプログラムで扱えるようにする
	DirectX::ScratchImage image{};
	std::wstring filePathW = ConvertString (filePath);
	HRESULT hr = DirectX::LoadFromWICFile (filePathW.c_str (), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert (SUCCEEDED (hr));

	// ミップマップの作製
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps (image.GetImages (), image.GetImageCount (), image.GetMetadata (), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert (SUCCEEDED (hr));

	// ミップマップ付きのデータを返す
	return mipImages;
}
//===============================================//

//===============================================//
// 2.DirectX12のTextureResourceを作る

ID3D12Resource* CreateTextureResource (ID3D12Device* device, const DirectX::TexMetadata& metadata) {
	//==================//
	// 2-1.metadataをもとにResourceの設定

	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT (metadata.width);	// Textureの幅
	resourceDesc.Height = UINT (metadata.height); // Textureの高さ
	resourceDesc.MipLevels = UINT16 (metadata.mipLevels); // mipmapの数
	resourceDesc.DepthOrArraySize = UINT16 (metadata.arraySize); // 奥行き or 配列Textureの配列数
	resourceDesc.Format = metadata.format; // TextureのFormat
	resourceDesc.SampleDesc.Count = 1; // サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION (metadata.dimension); // Textureの次元数。普段使っているのは2次元
	//==================//

	//==================//
	// 2-2.利用するヒープの設定

	// 非常に特殊な運用。
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT; // 細かい設定を行う
	//==================//

	//==================//
	// 2-3.Resourceを生成する

	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource (
		&heapProperties, // Heapの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定
		&resourceDesc, // Resourceの設定
		D3D12_RESOURCE_STATE_COPY_DEST, // 初回のResourceState。Textureは基本読むだけ
		nullptr, // Clear最適値。使わないのでnullptr
		IID_PPV_ARGS (&resource)); // 作成するResourceポインタへのポインタ
	assert (SUCCEEDED (hr));
	return resource;
	//==================//
}

//===============================================//


//===============================================//
// 3.TextureResourceにデータを転送する

[[nodiscard]]
ID3D12Resource* UploadTextureData (ID3D12Resource* texture, const DirectX::ScratchImage& mipImages, ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList) {
	std::vector<D3D12_SUBRESOURCE_DATA> subresources;
	DirectX::PrepareUpload (device, mipImages.GetImages (), mipImages.GetImageCount (), mipImages.GetMetadata (), subresources);
	uint64_t intermediateSize = GetRequiredIntermediateSize (texture, 0, UINT (subresources.size ()));
	ID3D12Resource* intermediateResource = CreateBufferResource (device, intermediateSize, "intermediateResource");
	UpdateSubresources (commandList, texture, intermediateResource, 0, 0, UINT (subresources.size ()), subresources.data ());
	//Textureへの転送後は利用できるよう、D3D12_RESOURCE_STATE_COPY_DESTからD3D12_RESOURCE_STATE_GENERIC_READへResourceStateを変更する
	D3D12_RESOURCE_BARRIER barrier{};
	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrier.Transition.pResource = texture;
	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
	commandList->ResourceBarrier (1, &barrier);
	return intermediateResource;
}
//===============================================//

//=================================================================================================//


//=================================================================================================//
// DepthStencilTextureを作る

ID3D12Resource* CreateDepthStencilTextureResource (ID3D12Device* device, int32_t width, int32_t height) {
	// 生成するResourceの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = width;	// Textureの幅
	resourceDesc.Height = height;	// Textureの高さ
	resourceDesc.MipLevels = 1;	// mipmapの数
	resourceDesc.DepthOrArraySize = 1;	// 奥行き or 配列Textureの配列数
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT; // DepthStencilとして利用可能なフォーマット
	resourceDesc.SampleDesc.Count = 1;	// サンプリングカウント。1固定
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;	// 2次元
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;	// DepthStencilとして使う通知

	// 利用するheapの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;	// VRAM上に作る

	// 深度値のクリア最適化設定
	D3D12_CLEAR_VALUE depthClearValue{};
	depthClearValue.DepthStencil.Depth = 1.f;	// 1.f（最大値）でクリア
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	// フォーマット。Resourceと合わせる

	// Resourceの生成
	ID3D12Resource* resource = nullptr;
	HRESULT hr = device->CreateCommittedResource (
		&heapProperties,	// Heapの設定
		D3D12_HEAP_FLAG_NONE,	// Heapの特殊な設定
		&resourceDesc,	// Resourceの設定
		D3D12_RESOURCE_STATE_DEPTH_WRITE,	// 深度値を書き込む状態にしておく
		&depthClearValue,	// Clear最適値
		IID_PPV_ARGS (&resource));	// 作成するResourceポインタへのポインタ
	assert (SUCCEEDED (hr));

	return resource;
}
//=================================================================================================//


//=================================================================================================//
// DescriptorHandle取得の関数

// CPUHandle
D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle (ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = descriptorHeap->GetCPUDescriptorHandleForHeapStart ();
	handleCPU.ptr += (descriptorSize * index);
	return handleCPU;
}

// GPUHandle
D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle (ID3D12DescriptorHeap* descriptorHeap, uint32_t descriptorSize, uint32_t index) {
	D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = descriptorHeap->GetGPUDescriptorHandleForHeapStart ();
	handleGPU.ptr += (descriptorSize * index);
	return handleGPU;
}

//=================================================================================================//


//===================================================================================================================================================//
// Windowsアプリでのエントリーポイント（main関数）
int WINAPI WinMain (_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR, _In_ int) {

	// COMの初期化
	CoInitializeEx (0, COINIT_MULTITHREADED);

	// 誰も捕捉しなかった場合に（Unhandled）、補足する関数を登録
	// main関数始まってすぐに登録するとよい
	SetUnhandledExceptionFilter (ExportDump);


	//=============================================================================================//
	// ディレクトリを掘る

	std::filesystem::create_directory ("logs");
	//=============================================================================================//


	//=============================================================================================//
	// 現在時刻でログファイルを生成する

	// 現在時刻を取得（UTC時刻）
	std::chrono::system_clock::time_point now = std::chrono::system_clock::now ();
	// ログファイルの名前にコンマ何秒はいらないので、削って秒にする
	std::chrono::time_point<std::chrono::system_clock, std::chrono::seconds>
		nowSeconds = std::chrono::time_point_cast<std::chrono::seconds>(now);
	// 日本時間（PCの設定時刻）に変換
	std::chrono::zoned_time localTime{ std::chrono::current_zone (),nowSeconds };
	// formatを使って年月日_時分秒の文字列に変換
	std::string dateString = std::format ("{:%Y%m%d_%H%M%S}", localTime);
	// 時刻を使ってファイル名を決定
	std::string logFilePath = std::string ("logs/") + dateString + ".log";
	// ファイルを作って書き込み準備
	std::ofstream logStream (logFilePath);
	//=============================================================================================//


	//=============================================================================================//
	// ウィンドウ作成

	WNDCLASS wc{};
	// ウィンドウプロシージャ
	wc.lpfnWndProc = WindowProc;
	// ウィンドウクラス名
	wc.lpszClassName = L"CG2WindowClass";
	// インスタンスハンドル
	wc.hInstance = GetModuleHandle (nullptr);
	// カーソル
	wc.hCursor = LoadCursor (nullptr, IDC_ARROW);

	// ウィンドウクラスを登録する
	RegisterClass (&wc);

	// クライアント領域のサイズ
	const int32_t kClientWidth = 1280;
	const int32_t kClientHeight = 720;

	// ウィンドウサイズを表す構造体にクライアント領域を入れる
	RECT wrc = { 0,0,kClientWidth,kClientHeight };

	// クライアント領域を元に実際のサイズにwrcを変更してもらう
	AdjustWindowRect (&wrc, WS_OVERLAPPEDWINDOW, false);

	// ウィンドウの生成
	HWND hwnd = CreateWindow (
		wc.lpszClassName,      // 利用するクラス名
		L"CG2",                // タイトルバーの文字
		WS_OVERLAPPEDWINDOW,   // よく見るウィンドウスタイル
		CW_USEDEFAULT,         // 表示X座標（Windowsに任せる）
		CW_USEDEFAULT,         // 表示Y座標（WindowsOSに任せる）
		wrc.right - wrc.left,  // ウィンドウ横幅
		wrc.bottom - wrc.top,  // ウィンドウ縦幅
		nullptr,               // 親ウィンドウハンドル
		nullptr,               // メニューハンドル
		wc.hInstance,          // インスタンスハンドル
		nullptr                // オプション
	);

	// ウィンドウを表示する
	ShowWindow (hwnd, SW_SHOW);
	Log (logStream, "Window生成！！");
	//=============================================================================================//


	//=============================================================================================//
	// Transform変数

	Transform transform{ {1.f,1.f,1.f},{0.f,0.f,0.f},{0.f,0.f,0.f} };
	Transform cameraTransform{ {1.f,1.f,1.f},{0.f,0.f,0.f},{0.f,0.f,-5.f} };
	//=============================================================================================//


	//=============================================================================================//
	// DebugLayer

#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED (D3D12GetDebugInterface (IID_PPV_ARGS (&debugController)))) {
		// デバッグレイヤーを有効化する
		debugController->EnableDebugLayer ();
		// さらにGPU側でもチェックを行うようにする
		debugController->SetEnableGPUBasedValidation (TRUE);
	}
#endif
	//=============================================================================================//


	//=============================================================================================//
	// DirectX12を初期化

	// DXGIファクトリーの生成
	IDXGIFactory7* dxgiFactory = nullptr;
	// HRESULTはWindows系のエラーコードであり、
	// 関数が成功したかどうかをSUCCEEDEDマクロで判定できる
	HRESULT hr = CreateDXGIFactory (IID_PPV_ARGS (&dxgiFactory));
	// 初期化の根本的な部分でエラーが出た場合はプログラムが間違っているか、
	// どうにもできない場合が多いのでassertしておく
	assert (SUCCEEDED (hr));
	// 使用するアダプタ用の変数。最初にnullptrを入れておく
	IDXGIAdapter4* useAdapter = nullptr;
	// 良い順にアダプタを積む
	for (UINT i = 0; dxgiFactory->EnumAdapterByGpuPreference (i,
		DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS (&useAdapter)) !=
		DXGI_ERROR_NOT_FOUND; ++i) {
		// アダプターの情報を取得する
		DXGI_ADAPTER_DESC3 adapterDesc{};
		hr = useAdapter->GetDesc3 (&adapterDesc);
		assert (SUCCEEDED (hr)); // 取得できないのは一大事
		// ソフトウェアアダプタでなければ採用！
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			// 採用したアダプタの情報をログに出力。wstringのほうなので注意
			Log (ConvertString (std::format (L"Use Adapter:{}\n", adapterDesc.Description)));
			break;
		}
		useAdapter = nullptr; // ソフトウェアアダプタの場合は見なかったことにする
	}
	// 適切なアダプタが見つからなかったので起動できない
	assert (useAdapter != nullptr);

	ID3D12Device* device = nullptr;
	// 機能レベルとログ出力用の文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,D3D_FEATURE_LEVEL_12_1,D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelStrings[] = { "12.2","12.1","12.0" };
	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof (featureLevels); ++i) {
		// 採用したアダプターでデバイスを生成
		hr = D3D12CreateDevice (useAdapter, featureLevels[i], IID_PPV_ARGS (&device));
		// 指定した機能レベルでデバイスが生成できたかを確認
		if (SUCCEEDED (hr)) {
			// 生成できたのでログ出力を行ってループを抜ける
			Log (std::format ("FeatureLevel : {}\n", featureLevelStrings[i]));
			break;
		}
	}
	// デバイスの生成がうまくいかなかったので起動できない
	assert (device != nullptr);
	Log ("Complete create D3D12Device!!!\n");// 初期化完了のログを出す
	Log (logStream, "DX12初期化完了！！");
	//=============================================================================================//


	//=============================================================================================//
	// エラー・警告、すなわち停止

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED (device->QueryInterface (IID_PPV_ARGS (&infoQueue)))) {
		// やばいエラー時に止まる
		infoQueue->SetBreakOnSeverity (D3D12_MESSAGE_SEVERITY_CORRUPTION, true);
		// エラー時に止まる
		infoQueue->SetBreakOnSeverity (D3D12_MESSAGE_SEVERITY_ERROR, true);
		// 警告時に止まる
		infoQueue->SetBreakOnSeverity (D3D12_MESSAGE_SEVERITY_WARNING, true);
		// 解放
		infoQueue->Release ();
	}
#endif
	//=============================================================================================//


	//=============================================================================================//
	//	エラーと警告の抑制
#ifdef _DEBUG
	// 抑制するメッセージのID
	D3D12_MESSAGE_ID denyIds[] = {
		// Windows11でのDXGIデバッグレイヤーとDX12デバッグレイヤーの相互作用バグによるエラーメッセージ
		// https://stackoverflow.com/questions/69805245/directx-12-application-is-crashing-in-windows-11
		D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE
	};
	// 抑制するレベル
	D3D12_MESSAGE_SEVERITY severities[] = { D3D12_MESSAGE_SEVERITY_INFO };
	D3D12_INFO_QUEUE_FILTER filter{};
	filter.DenyList.NumIDs = _countof (denyIds);
	filter.DenyList.pIDList = denyIds;
	filter.DenyList.NumSeverities = _countof (severities);
	filter.DenyList.pSeverityList = severities;
	// 指定したメッセージの表示を抑制する
	infoQueue->PushStorageFilter (&filter);
	Log (logStream, "エラーと警告の抑制○！！");
#endif
	//=============================================================================================//


	//=============================================================================================//
	// コマンドキューを生成する

	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};
	hr = device->CreateCommandQueue (&commandQueueDesc,
		IID_PPV_ARGS (&commandQueue));
	// コマンドキューの生成がうまくいかなかったので起動できない
	assert (SUCCEEDED (hr));
	Log (logStream, "コマンドキュー生成！！");
	//=============================================================================================//


	//=============================================================================================//
	// コマンドアロケータを生成する

	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS (&commandAllocator));
	// コマンドアロケータの生成がうまくいかなかったので起動できない
	assert (SUCCEEDED (hr));
	Log (logStream, "コマンドアロケータ生成！！");
	//=============================================================================================//


	//=============================================================================================//
	// コマンドリストを生成する

	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr,
		IID_PPV_ARGS (&commandList));
	// コマンドリストの生成がうまくいかなかったので起動できない
	assert (SUCCEEDED (hr));
	Log (logStream, "コマンドリスト生成！！");
	//=============================================================================================//


	//=============================================================================================//
	// スワップチェーンを生成する

	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;	// 画面の幅。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Height = kClientHeight; // 画面の高さ。ウィンドウのクライアント領域を同じものにしておく
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式
	swapChainDesc.SampleDesc.Count = 1; //マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // 描画のターゲットとして利用する
	swapChainDesc.BufferCount = 2; // ダブルバッファ
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // モニターにうつしたら、中身を破棄
	// コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd (commandQueue, hwnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert (SUCCEEDED (hr));
	Log (logStream, "SwapChain生成！！");
	//=============================================================================================//


	//=============================================================================================//
	// ディスクリプタヒープの生成

	// DescriptorSizeを取得しておく
	const uint32_t descriptorSizeSRV = device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	const uint32_t descriptorSizeRTV = device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	const uint32_t descriptorSizeDSV = device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// RTV用のヒープでディスクリプタの数は2。RTVはShader内で触るものではないので、ShaderVisibleはfalse
	ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap (device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false, "rtvDescriptorHeap");
	// SRV用のヒープでディスクリプタの数は128。SRVはShader内で触るものなので、ShaderVisibleはtrue
	ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap (device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true, "srvDescriptorHeap");
	Log (logStream, "DescriptHeap生成！！");
	//=============================================================================================//


	//=============================================================================================//
	// SwapChainからResourceを引っ張ってくる

	ID3D12Resource* swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer (0, IID_PPV_ARGS (&swapChainResources[0]));
	// うまく取得できなければ起動できない
	assert (SUCCEEDED (hr));
	hr = swapChain->GetBuffer (1, IID_PPV_ARGS (&swapChainResources[1]));
	assert (SUCCEEDED (hr));
	Log (logStream, "SwapChainからResourceひっぱった！！");
	//=============================================================================================//


	//=============================================================================================//
	// RTVの設定

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; //出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2dテクスチャとして読み込む
	// ディスクリプタの先頭を取得する
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart ();
	// RTVを2つ作るのでディスクリプタを2つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[2];
	// まず1つ目を作る。1つ目は最初のところに作る
	rtvHandles[0] = GetCPUDescriptorHandle (rtvDescriptorHeap, descriptorSizeRTV, 0);
	device->CreateRenderTargetView (swapChainResources[0], &rtvDesc, rtvHandles[0]);
	// 2つ目を作る
	rtvHandles[1] = GetCPUDescriptorHandle (rtvDescriptorHeap, descriptorSizeRTV, 1);
	device->CreateRenderTargetView (swapChainResources[1], &rtvDesc, rtvHandles[1]);
	Log (logStream, "RTV設定したよ！！");
	//=============================================================================================//


	//=============================================================================================//
	// FenceとEventを生成する

	// 初期値0でFenceを作る
	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence (fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&fence));
	assert (SUCCEEDED (hr));

	// FenceのSignalを待つためのイベントを作成する
	HANDLE fenceEvent = CreateEvent (NULL, FALSE, FALSE, NULL);
	assert (fenceEvent != nullptr);
	Log (logStream, "FenceとEvent生成した！！！");
	//=============================================================================================//


	//=============================================================================================//
	// DXCの初期化

	//dxCompilerを初期化
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance (CLSID_DxcUtils, IID_PPV_ARGS (&dxcUtils));
	assert (SUCCEEDED (hr));
	hr = DxcCreateInstance (CLSID_DxcCompiler, IID_PPV_ARGS (&dxcCompiler));
	assert (SUCCEEDED (hr));
	Log (logStream, "dxCompiler初期化でけた！！");

	// 現時点でincludeはしないが、includeに対応するための設定を行っておく
	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler (&includeHandler);
	assert (SUCCEEDED (hr));
	Log (logStream, "DXC初期化でけた！！");
	//=============================================================================================//


	//=============================================================================================//
	// ImGuiの初期化

#ifdef USE_IMGUI
	// ImGuiの初期化
	IMGUI_CHECKVERSION ();
	ImGui::CreateContext ();
	ImGui::StyleColorsDark ();
	ImGui_ImplWin32_Init (hwnd);
	ImGui_ImplDX12_Init (device,
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap,
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart (),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart ());
	ImGuiIO& io = ImGui::GetIO ();
	io.Fonts->Build ();
	Log (logStream, "ImGui初期化でけた！！");
#endif
	//=============================================================================================//


	//=============================================================================================//  <-CG2_02_00
	// PSO


	//===========================================//
	// RootSignatureを生成する

	// RootSignature作成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DescriptorTable
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0;	// 0から始まる
	descriptorRange[0].NumDescriptors = 1;	// 数は1つ
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;	// SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;	//Offsetを自動計算

	// RootParameter作成。PixelShaderのMaterialとVertexShaderのTransform
	D3D12_ROOT_PARAMETER rootParameters[4] = {};
	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// CBVを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	// PixelShaderで使う
	rootParameters[0].Descriptor.ShaderRegister = 0;	// レジスタ番号0を使う
	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// CBVを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;	// VertexShaderで使う
	rootParameters[1].Descriptor.ShaderRegister = 0;	// レジスタ番号0を使う
	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;	// Descriptorを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	// PixelShaderで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange;	// Tableの中身の配列を指定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof (descriptorRange);	// Tableで利用する数
	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;	// CBVを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	// PixelShaderで使う
	rootParameters[3].Descriptor.ShaderRegister = 1;	// レジスタ番号1を使う
	descriptionRootSignature.pParameters = rootParameters;	// ルートパラメータ配列へのポインタ
	descriptionRootSignature.NumParameters = _countof (rootParameters);	// 配列の長さ


	//==================//
	// Samplerの設定

	D3D12_STATIC_SAMPLER_DESC staticSamplers[1] = {};
	staticSamplers[0].Filter = D3D12_FILTER_MIN_POINT_MAG_MIP_LINEAR;	// バイリニアフィルタ
	staticSamplers[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;	// 0~1の範囲をリピート
	staticSamplers[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	staticSamplers[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;	// 比較しない
	staticSamplers[0].MaxLOD = D3D12_FLOAT32_MAX;	// ありったけのMipmapを使う
	staticSamplers[0].ShaderRegister = 0;	// レジスタ番号0を使う
	staticSamplers[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	// PixelShaderで使う
	descriptionRootSignature.pStaticSamplers = staticSamplers;
	descriptionRootSignature.NumStaticSamplers = _countof (staticSamplers);
	//==================//


	// シリアライズしてバイナリにする
	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature (&descriptionRootSignature,
		D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED (hr)) {
		Log (reinterpret_cast<char*>(errorBlob->GetBufferPointer ()));
		assert (false);
	}

	// バイナリをもとに作成
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature (0,
		signatureBlob->GetBufferPointer (), signatureBlob->GetBufferSize (),
		IID_PPV_ARGS (&rootSignature));
	assert (SUCCEEDED (hr));
	Log (logStream, "RootSignature生成！！");
	//===========================================//


	//===========================================//
	// InputLayout

	//InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[3] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[2].SemanticName = "NORMAL";
	inputElementDescs[2].SemanticIndex = 0;
	inputElementDescs[2].Format = DXGI_FORMAT_R32G32B32_FLOAT;
	inputElementDescs[2].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof (inputElementDescs);
	Log (logStream, "InputLayout生成！！");
	//===========================================//


	//===========================================//
	// BlendState

	// BlendStateの設定
	D3D12_BLEND_DESC blendDesc{};
	// すべての色要素を書き込む
	blendDesc.RenderTarget[0].RenderTargetWriteMask =
		D3D12_COLOR_WRITE_ENABLE_ALL;
	Log (logStream, "BlendState生成！！");
	//===========================================//


	//===========================================//
	// RasterizerState

	// RasterizerStateの設定
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 裏面（時計回り）を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	Log (logStream, "RasterizerState生成！！");
	//===========================================//


	//===========================================//
	// ShaderをCompileする 

	// Shaderをコンパイルする
	IDxcBlob* vertexShaderBlob = CompileShader (L"Object3d.VS.hlsl",
		L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert (vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader (L"Object3d.PS.hlsl",
		L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert (pixelShaderBlob != nullptr);
	Log (logStream, "Shaderコンパイルできてるよ！");
	//===========================================//


	//===========================================//
	// DepthStencilStateの設定

	D3D12_DEPTH_STENCIL_DESC depthStencilDesc{};
	// depthの機能を有効化にする
	depthStencilDesc.DepthEnable = true;
	// 書き込みします
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	// 比較関数はLessEqual。つまり、近ければ描画される
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
	//===========================================//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature;// RootSignature
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;// InputLayout
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer (),
	vertexShaderBlob->GetBufferSize () };// VertexShader
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer (),
	pixelShaderBlob->GetBufferSize () };// PixelShader
	graphicsPipelineStateDesc.BlendState = blendDesc;// BlendState
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;// RasterizerState
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType =
		D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定（気にしなくて良い）
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// DepthStencilの設定
	graphicsPipelineStateDesc.DepthStencilState = depthStencilDesc;
	graphicsPipelineStateDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	// 実際に生成
	ID3D12PipelineState* graphicPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState (&graphicsPipelineStateDesc,
		IID_PPV_ARGS (&graphicPipelineState));
	assert (SUCCEEDED (hr));
	Log (logStream, "PSOでけた！！！");
	//=============================================================================================//


	//=============================================================================================//	
	// DepthStencilView

	// DSV用のヒープでディスクリプタの数は1。DSVはShader内で触るものではないので、ShaderVisibleはfalse
	ID3D12DescriptorHeap* dsvDescriptorHeap = CreateDescriptorHeap (device, D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false, "dsvDescriptorHeap");

	// DepthStencilTextureをウィンドウのサイズで作成
	ID3D12Resource* depthStencilResource = CreateDepthStencilTextureResource (device, kClientWidth, kClientHeight);
	// DSVの設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;	// Format。基本的にはResourceに合わせる
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;	// 2dTexture
	// DSVHeapの先頭にDSVを作る
	device->CreateDepthStencilView (depthStencilResource, &dsvDesc, dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart ());
	//=============================================================================================//


	//=============================================================================================//  <-CG2_02_00
	// VertexResourceを生成する

	// 実際に頂点リソースを作る
	ID3D12Resource* vertexResource = CreateBufferResource (device, sizeof (VertexData) * kVertexCount, "vertexResource");
	//=============================================================================================//


	//=============================================================================================//  <-CG2_02_00
	// VertexBufferViewを作成 

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	Log (logStream, "vertexBufferView生成！！");
	// リソースの先頭アドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress ();
	// 使用するリソースのサイズは頂点3つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof (VertexData) * kVertexCount;
	// 1頂点あたりのサイズ
	vertexBufferView.StrideInBytes = sizeof (VertexData);
	Log (logStream, "vertexBufferView作成！！");
	//=============================================================================================//


	//=============================================================================================//
	// Material用のResourceを作る

	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズを用意する
	ID3D12Resource* materialResource = CreateBufferResource (device, sizeof (Material), "materialResource");
	// マテリアルにデータを書き込む
	Material* materialData = nullptr;
	// 書き込むためのアドレスを取得
	materialResource->Map (0, nullptr, reinterpret_cast<void**>(&materialData));
	// 白色透明
	materialData->color = Vector4 (1.f, 1.f, 1.f, 1.f);
	// Lightingを有効化
	materialData->enableLighting = true;

	// Sprite用のMaterialを作成
	ID3D12Resource* materialResourceSprite = CreateBufferResource (device, sizeof (Material), "materialResourceSprite");
	// データ書き込み
	Material* materialDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	materialResourceSprite->Map (0, nullptr, reinterpret_cast<void**>(&materialDataSprite));
	// 白色
	materialDataSprite->color = Vector4 (1.f, 1.f, 1.f, 1.f);
	// Lightingを有効化
	materialDataSprite->enableLighting = false;
	//=============================================================================================//


	//=============================================================================================//
	// DirectionalLight用のResourceを作る

	// リソース作成
	ID3D12Resource* directionalLightResource = CreateBufferResource (device, sizeof (DirectionalLight), "directionalLightResource");
	// データを書き込む
	DirectionalLight* directionalLightData = nullptr;
	// 書き込むためのアドレスを取得
	directionalLightResource->Map (0, nullptr, reinterpret_cast<void**>(&directionalLightData));
	// デフォルト値
	directionalLightData->color = Vector4 (1.0f, 1.0f, 1.0f, 1.0f);
	directionalLightData->direction = Vector3 (0.0f, -1.0f, 0.0f);
	directionalLightData->intensity = 1.0f;
	//=============================================================================================//


	//=============================================================================================//
	// TransformationMatrix用のResourceを作る

	// WVP用のリソースを作る。Matrix4x4 1つ分のサイズを用意する
	ID3D12Resource* wvpResource = CreateBufferResource (device, sizeof (TransformationMatrix), "wvpResource");
	// データを書き込む
	TransformationMatrix* wvpData = nullptr;
	// 書き込むためのアドレスを取得
	wvpResource->Map (0, nullptr, reinterpret_cast<void**>(&wvpData));
	// 単位行列を書き込んでおく
	wvpData->World = MT3::MakeIdentity4x4 ();
	wvpData->WVP = MT3::MakeIdentity4x4 ();
	//=============================================================================================//


	//=============================================================================================//
	// IndexResource

	ID3D12Resource* indexResource =	CreateBufferResource (device, sizeof (uint32_t) * kIndexCount, "indexResource");
	D3D12_INDEX_BUFFER_VIEW indexBufferView{};
	// リソースの先頭アドレスから使う
	indexBufferView.BufferLocation = indexResource->GetGPUVirtualAddress ();
	// 使用するリソースのサイズはkIndexCount分のサイズ
	indexBufferView.SizeInBytes = sizeof (uint32_t) * kIndexCount;
	// インデックスはuint32_tとする
	indexBufferView.Format = DXGI_FORMAT_R32_UINT;
	// インデックスリソースにデータを書き込む
	uint32_t* indexData = nullptr;
	indexResource->Map (0, nullptr, reinterpret_cast<void**>(&indexData));
	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {
		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {
			uint32_t start = (latIndex * kSubdivision + lonIndex) * 6;

			uint32_t a = latIndex * (kSubdivision + 1) + lonIndex;
			uint32_t b = (latIndex + 1) * (kSubdivision + 1) + lonIndex;
			uint32_t c = latIndex * (kSubdivision + 1) + lonIndex + 1;
			uint32_t d = (latIndex + 1) * (kSubdivision + 1) + lonIndex + 1;

			indexData[start + 0] = a;
			indexData[start + 1] = b;
			indexData[start + 2] = c;

			indexData[start + 3] = c;
			indexData[start + 4] = b;
			indexData[start + 5] = d;
		}
	}
	//=============================================================================================//


	//=============================================================================================//
	// IndexResourceSprite

	ID3D12Resource* indexResourceSprite = CreateBufferResource (device, sizeof (uint32_t) * 6, "indexResourceSprite");
	D3D12_INDEX_BUFFER_VIEW indexBufferViewSprite{};
	// リソースの先頭アドレスから使う
	indexBufferViewSprite.BufferLocation = indexResourceSprite->GetGPUVirtualAddress ();
	// 使用するリソースのサイズはインデックス6つ分のサイズ
	indexBufferViewSprite.SizeInBytes = sizeof (uint32_t) * 6;
	// インデックスはuint32_tとする
	indexBufferViewSprite.Format = DXGI_FORMAT_R32_UINT;
	// インデックスリソースにデータを書き込む
	uint32_t* indexDataSprite = nullptr;
	indexResourceSprite->Map (0, nullptr, reinterpret_cast<void**>(&indexDataSprite));
	indexDataSprite[0] = 0;	indexDataSprite[1] = 1;	indexDataSprite[2] = 2;
	indexDataSprite[3] = 1; indexDataSprite[4] = 3; indexDataSprite[5] = 2;
	//=============================================================================================//


	//=============================================================================================//  <-CG2_02_00
	// Resourceにデータを書き込む

	// 頂点リソースにデータ書き込む
	VertexData* vertexData = nullptr;
	Log (logStream, "vertexData生成！！");
	// 書き込むためのアドレスを取得
	vertexResource->Map (0, nullptr, reinterpret_cast<void**>(&vertexData));

	//=============================================================================================//


	//=============================================================================================//  <-CG2_02_00
	// ViewportとScissor

	// ビューポート
	D3D12_VIEWPORT viewport{};
	Log (logStream, "viewPort生成！！");
	// クライアント領域のサイズと一緒にして画面全体
	viewport.Width = kClientWidth;
	viewport.Height = kClientHeight;
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	// Scissor矩形
	D3D12_RECT scissorRect{};
	// 基本的にビューポートと同じ矩形が構成されるようにする
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;
	//=============================================================================================//

	// Textureを読んで転送する
	DirectX::ScratchImage mipImages = LoadTexture ("resource/uvChecker.png");
	const DirectX::TexMetadata& metadata = mipImages.GetMetadata ();
	ID3D12Resource* textureResource = CreateTextureResource (device, metadata);
	ID3D12Resource* intermediateResource = UploadTextureData (textureResource, mipImages, device, commandList);
	// 2枚目のTextureを読んで転送する
	DirectX::ScratchImage mipImages2 = LoadTexture ("resource/white1x1.png");
	const DirectX::TexMetadata& metadata2 = mipImages.GetMetadata ();
	ID3D12Resource* textureResource2 = CreateTextureResource (device, metadata2);
	ID3D12Resource* intermediateResource2 = UploadTextureData (textureResource2, mipImages2, device, commandList);
	// コマンドリストを閉じる
	commandList->Close ();
	// コマンドリストを実行
	ID3D12CommandList* commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists (1, commandLists);
	fenceValue++;
	commandQueue->Signal (fence, fenceValue);
	if (fence->GetCompletedValue () < fenceValue) {
		HANDLE fenceEvent = CreateEvent (nullptr, false, false, nullptr);
		fence->SetEventOnCompletion (fenceValue, fenceEvent);
		WaitForSingleObject (fenceEvent, INFINITE);
		CloseHandle (fenceEvent);
	}

	// 次のコマンドを積めるようにリセット
	commandAllocator->Reset ();
	assert (SUCCEEDED (hr));
	commandList->Reset (commandAllocator, nullptr);
	assert (SUCCEEDED (hr));

	// 転送用リソースはもう不要
	intermediateResource->Release ();
	intermediateResource2->Release ();


	//=============================================================================================//
	// ShaderResourceViewを作る

	// metaDataを基にSRVの設定（1枚目）
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;// 2Dテクスチャ
	srvDesc.Texture2D.MipLevels = UINT (metadata.mipLevels);

	// metaDataを基にSRVの設定（2枚目）
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc2{};
	srvDesc2.Format = metadata2.format;
	srvDesc2.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc2.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;// 2Dテクスチャ
	srvDesc2.Texture2D.MipLevels = UINT (metadata2.mipLevels);

	// SRVを作成するDescriptorHeapの場所を決める
	// 先頭はImGuiが使っているのでその次を使う
	// 1枚目
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = GetCPUDescriptorHandle (srvDescriptorHeap, descriptorSizeSRV, 1);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = GetGPUDescriptorHandle (srvDescriptorHeap, descriptorSizeSRV, 1);
	// 2枚目
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU2 = GetCPUDescriptorHandle (srvDescriptorHeap, descriptorSizeSRV, 2);
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU2 = GetGPUDescriptorHandle (srvDescriptorHeap, descriptorSizeSRV, 2);
	// SRVの生成
	device->CreateShaderResourceView (textureResource, &srvDesc, textureSrvHandleCPU);
	device->CreateShaderResourceView (textureResource2, &srvDesc2, textureSrvHandleCPU2);
	//=============================================================================================//


	//=============================================================================================//
	// Spriteを表示させる

	// Sprite用の頂点リソースを作る
	ID3D12Resource* vertexResourceSprite = CreateBufferResource (device, sizeof (VertexData) * 4, "vertexResourceSprite");
	// 頂点バッファービューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	// リソースの先頭のアドレスから使う
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress ();
	// 使用するリソースのサイズは頂点4つ分のサイズ
	vertexBufferViewSprite.SizeInBytes = sizeof (VertexData) * 4;
	// 1頂点あたりのサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof (VertexData);

	// 頂点データを設定する
	VertexData* vertexDataSprite = nullptr;
	vertexResourceSprite->Map (0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));
	// 左下->0
	vertexDataSprite[0].position = { 0.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[0].normal = { 0.0f,0.0f,-1.0f };

	// 左上->1
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[1].normal = { 0.0f,0.0f,-1.0f };

	// 右下->2
	vertexDataSprite[2].position = { 640.0f, 360.0f, 0.0f, 1.0f };
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };
	vertexDataSprite[2].normal = { 0.0f,0.0f,-1.0f };

	// 右上->3
	vertexDataSprite[3].position = { 640.0f, 0.0f, 0.0f, 1.0f };
	vertexDataSprite[3].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[3].normal = { 0.0f,0.0f,-1.0f };

	// Transform周りを作る
	// Sprite用のTransformationMatrix用のリソースを作る。Matrix4x4 1つ分のサイズ
	ID3D12Resource* transformationMatrixResourceSprite = CreateBufferResource (device, sizeof (TransformationMatrix), "transformationMatrixResourceSprite");
	// データを書きこむ
	TransformationMatrix* transformationMatrixDataSprite = nullptr;
	// 書き込むためのアドレスを取得
	transformationMatrixResourceSprite->Map (0, nullptr, reinterpret_cast<void**>(&transformationMatrixDataSprite));
	// 単位行列を書き込んでおく
	transformationMatrixDataSprite->World = MT3::MakeIdentity4x4 ();
	transformationMatrixDataSprite->WVP = MT3::MakeIdentity4x4 ();
	// CPUで動かす用のTransform
	Transform transformSprite = { {1.0f,1.0f,1.0f},{0.0f,0.0f,0.0f},{0.0f,0.0f,0.0f} };

	// WVPMatrixを作って書き込む
	Matrix4x4 worldMatrixSprite = MT3::MakeAffineMatrix (transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	Matrix4x4 viewMatrixSprite = MT3::MakeIdentity4x4 ();
	Matrix4x4 projectionMatrixSprite = MT3::MakeOrthographicMatrix (0.0f, 0.0f, float (kClientWidth), float (kClientHeight), 0.0f, 100.0f);
	Matrix4x4 worldViewProjectionMatrixSprite = MT3::Multiply (worldMatrixSprite, MT3::Multiply (viewMatrixSprite, projectionMatrixSprite));
	transformationMatrixDataSprite->World = worldMatrixSprite;
	transformationMatrixDataSprite->WVP = worldViewProjectionMatrixSprite;
	//=============================================================================================//

	bool useWhite1x1 = true;

	Log (logStream, "メインループに入れた！");
	Log (std::format ("ALL CLEAR!!!\n", hr));
	//-------------
	// メインループ
	//------------/
	MSG msg{};
	// ウィンドウのxボタンが押されるまでループ
	while (msg.message != WM_QUIT) {
		// Windowにメッセージが来てたら最優先で処理させる
		if (PeekMessage (&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		} else {


			// 経度分割1つ分の角度
			const float kLonEvery = std::numbers::pi_v<float> *2.f / float (kSubdivision);
			// 緯度分割1つ分の角度
			const float kLatEvery = std::numbers::pi_v<float> / float (kSubdivision);
			// 緯度の方向に分割
			for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex) {
				float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;
				// 軽度の方向に分割
				for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) {

					uint32_t index = latIndex * (kSubdivision + 1) + lonIndex;

					float lon = lonIndex * kLonEvery;

					vertexData[index].position.x = cos (lat) * cos (lon);
					vertexData[index].position.y = sin (lat);
					vertexData[index].position.z = cos (lat) * sin (lon);
					vertexData[index].position.w = 1.0f;
					vertexData[index].texcoord = { float (lonIndex) / float (kSubdivision),	1.0f - float (latIndex) / float (kSubdivision) };
					vertexData[index].normal = { vertexData[index].position.x, vertexData[index].position.y, vertexData[index].position.z };
				}
			}

			transform.rotate.y += 0.01f;

			Matrix4x4 worldMatrix = MT3::MakeAffineMatrix (transform.scale, transform.rotate, transform.translate);
			Matrix4x4 cameraMatrix = MT3::MakeAffineMatrix (cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			Matrix4x4 viewMatrix = MT3::Inverse (cameraMatrix);
			Matrix4x4 projectionMatrix = MT3::MakePerspectiveFovMatrix (0.45f, float (kClientWidth) / float (kClientHeight), 0.1f, 100.f);
			Matrix4x4 worldViewProjectionMatrix = MT3::Multiply (worldMatrix, MT3::Multiply (viewMatrix, projectionMatrix));

			wvpData->World = worldMatrix;
			wvpData->WVP = worldViewProjectionMatrix;

		#ifdef USE_IMGUI
			// ImGui<<フレーム始まるよ
			ImGui_ImplDX12_NewFrame ();
			ImGui_ImplWin32_NewFrame ();
			ImGui::NewFrame ();

			ImGui::ShowDemoWindow ();

			ImGui::Checkbox ("useWhite1x1", &useWhite1x1);

			// ImGuiの内部コマンドを生成する
			ImGui::Render ();
		#endif
			//=====================================================================================//
			// これから書き込むバックバッファのインデックスを取得

			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex ();


			//===================================//
			// TransitionBarrierを張る

			// TransitionBarrierの設定
			D3D12_RESOURCE_BARRIER barrier{};
			// 今回のバリアはTransition
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			// Noneにしておく
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			// バリアを貼る対象のリソース。現在のバックバッファにしておく
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			// 遷移前（現在）のResourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			// 遷移後のResourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			// TransitionBarrierを貼る
			commandList->ResourceBarrier (1, &barrier);
			//===================================//


			// 描画用のDescriptorHeapの設定
			ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps (1, descriptorHeaps);
			// 描画先のRTVとDSVを設定する
			D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = dsvDescriptorHeap->GetCPUDescriptorHandleForHeapStart ();
			commandList->OMSetRenderTargets (1, &rtvHandles[backBufferIndex], false, &dsvHandle);
			// 指定した色で画面全体をクリアする
			float clearColor[] = { 0.1f,0.25f,0.5f,1.f }; // 青っぽい色。RGBAの順
			commandList->ClearRenderTargetView (rtvHandles[backBufferIndex], clearColor, 0, nullptr);
			commandList->ClearDepthStencilView (dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.f, 0, 0, nullptr);

			//===================================//
			// コマンドを積む

			commandList->RSSetViewports (1, &viewport);	// viewportを設定
			commandList->RSSetScissorRects (1, &scissorRect);	// Scissorを設定
			// RootSignatureを設定。PS0に設定しているけど別途設定が必要
			commandList->SetGraphicsRootSignature (rootSignature);
			commandList->SetPipelineState (graphicPipelineState);	// PS0を設定
			commandList->IASetVertexBuffers (0, 1, &vertexBufferView);	// VBVを設定
			// 形状を設定。PS0に設定しているものとはまた別。同じものを設定すると考えておけばよい
			commandList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView (0, materialResource->GetGPUVirtualAddress ());
			// wvp用のCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView (1, wvpResource->GetGPUVirtualAddress ());
			// SRVのDescriptorTableの先頭を設定。2はrootParameter[2]である
			commandList->SetGraphicsRootDescriptorTable (2, useWhite1x1 ? textureSrvHandleGPU2 : textureSrvHandleGPU);
			// Lighting
			commandList->SetGraphicsRootConstantBufferView (3, directionalLightResource->GetGPUVirtualAddress ());
			// 描画！（DrawCall/ドローコール）。3頂点で1つのインスタンス。インスタンスについては今後
			commandList->IASetIndexBuffer (&indexBufferView);	// IBVを設定
			commandList->DrawIndexedInstanced (kIndexCount, 1, 0, 0, 0);
			commandList->SetGraphicsRootDescriptorTable (2, textureSrvHandleGPU);
			// Spriteの描画。変更が必要なものだけ変更する
			commandList->IASetVertexBuffers (0, 1, &vertexBufferViewSprite);	// VBVを設定
			commandList->IASetIndexBuffer (&indexBufferViewSprite);	// IBVを設定
			// マテリアルCBufferの場所を設定
			commandList->SetGraphicsRootConstantBufferView (0, materialResourceSprite->GetGPUVirtualAddress ());
			// TransformationMatrixCBufferの場所を特定
			commandList->SetGraphicsRootConstantBufferView (1, transformationMatrixResourceSprite->GetGPUVirtualAddress ());
			// 描画！（DrawCall/ドローコール）
			commandList->DrawIndexedInstanced (6, 1, 0, 0, 0);
			//===================================//


			//===================================//
			// ImGuiを描画する

		#ifdef USE_IMGUI
					// 実際のcommandListのImGuiの描画コマンドを積む
			ImGui_ImplDX12_RenderDrawData (ImGui::GetDrawData (), commandList);
		#endif
			//===================================//


			//===================================//
			// 画面表示できるようにする

			// 画面に描く処理はすべて終わり、画面に映すので、状態を遷移
			// 今回はRenderTargetからPresentする
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			// TransitionBarrierを張る
			commandList->ResourceBarrier (1, &barrier);
			//===================================//


			// コマンドリストの内容を確定させる。すべてのコマンドを積んでからCloseすること
			hr = commandList->Close ();
			assert (SUCCEEDED (hr));
			//=====================================================================================//


			//=====================================================================================//
			// コマンドをキックする

			// GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists (1, commandLists);
			// GPUとOSに画面の交換を行うよう通知する
			swapChain->Present (1, 0);


			//===================================//
			// GPUにSignalを送る

			// Fenceの値を更新
			fenceValue++;
			// GPUがここまでたどり着いたときに、Fenceの値を指定した値に代入するようにSignalを送る
			commandQueue->Signal (fence, fenceValue);


			//==========//
			// Fenceの値を確認してGPUを待つ

			// Fenceの値が指定したSignal値にたどり着いているか確認する
			//GetCompletedValueの初期値はFence作成時に渡した初期値
			if (fence->GetCompletedValue () < fenceValue) {
				// 指定したSignalにたどり着いていないので、たどり着くまで待つようにイベントを設定する
				fence->SetEventOnCompletion (fenceValue, fenceEvent);
				// イベント待つ
				WaitForSingleObject (fenceEvent, INFINITE);
			}
			//==========//

			//===================================//


			// 次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset ();
			assert (SUCCEEDED (hr));
			hr = commandList->Reset (commandAllocator, nullptr);
			assert (SUCCEEDED (hr));
			//=====================================================================================//
		}
	}

	//=============================================================================================//
	// ImGuiの終了処理
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown ();
	ImGui_ImplWin32_Shutdown ();
	ImGui::DestroyContext ();
#endif
	//=============================================================================================//

	// COMの終了処理
	CoUninitialize ();

	//=============================================================================================//
	// DX12のオブジェクトを解放

	// リソース
	vertexResource->Release ();
	vertexResourceSprite->Release ();
	materialResource->Release ();
	materialResourceSprite->Release ();
	wvpResource->Release ();
	textureResource->Release ();
	textureResource2->Release ();
	depthStencilResource->Release ();
	transformationMatrixResourceSprite->Release ();
	directionalLightResource->Release ();
	indexResource->Release ();
	indexResourceSprite->Release ();

	// パイプライン系
	graphicPipelineState->Release ();
	rootSignature->Release ();

	// Shader
	vertexShaderBlob->Release ();
	pixelShaderBlob->Release ();

	signatureBlob->Release ();

	if (errorBlob) {
		errorBlob->Release ();
	}

	// DXC
	includeHandler->Release ();
	dxcCompiler->Release ();
	dxcUtils->Release ();

	// コマンド系
	commandList->Release ();
	commandAllocator->Release ();
	commandQueue->Release ();

	// SwapChain系
	swapChainResources[0]->Release ();
	swapChainResources[1]->Release ();
	swapChain->Release ();

	// DescriptorHeap
	rtvDescriptorHeap->Release ();
	srvDescriptorHeap->Release ();
	dsvDescriptorHeap->Release ();

	// Fence
	fence->Release ();
	CloseHandle (fenceEvent);

	// Adapter / Factory
	useAdapter->Release ();
	dxgiFactory->Release ();

#ifdef _DEBUG
	debugController->Release ();
#endif

	// device
	device->Release ();

	// リソースチェック
	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED (DXGIGetDebugInterface1 (0, IID_PPV_ARGS (&debug)))) {
		debug->ReportLiveObjects (DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects (DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects (DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release ();
	}

	CloseWindow (hwnd);

	//=============================================================================================//
	return 0;
}
//===================================================================================================================================================//