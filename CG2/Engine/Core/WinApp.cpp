#include "WinApp.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#include "../../../externals/imgui/imgui_impl_win32.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam);
#endif

//=============================================================================================//
// 初期化

void WinApp::Initialize() {

	// WindowClass設定
	wc_.lpfnWndProc = WindowProc;
	wc_.lpszClassName = L"CG2WindowClass";
	wc_.hInstance = GetModuleHandle(nullptr);
	wc_.hCursor = LoadCursor(nullptr, IDC_ARROW);

	// WindowClass登録
	RegisterClass(&wc_);

	// WindowSize設定
	RECT wrc = { 0,	0,	kClientWidth,	kClientHeight };

	// ClientSizeからWindowSizeへ変換
	AdjustWindowRect(
		&wrc,
		WS_OVERLAPPEDWINDOW,
		false);

	// Window生成
	hwnd_ = CreateWindow(
		wc_.lpszClassName,
		L"CG2",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		wrc.right - wrc.left,
		wrc.bottom - wrc.top,
		nullptr,
		nullptr,
		wc_.hInstance,
		nullptr);

	// Window表示
	ShowWindow(hwnd_, SW_SHOW);
}

//=============================================================================================//
// 更新

void WinApp::Update() {

}

//=============================================================================================//
// メッセージの処理

bool WinApp::ProcessMessage() {

	MSG msg{};

	if (PeekMessage(
		&msg,
		nullptr,
		0,
		0,
		PM_REMOVE)) {

		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (msg.message == WM_QUIT) {
		return true;
	}

	return false;
}

//=============================================================================================//
// 終了処理

void WinApp::Finalize() {
	// Windowを閉じる
	CloseWindow(hwnd_);
	CoUninitialize();
}

//=============================================================================================//
// ウィンドウプロシージャ

LRESULT CALLBACK WinApp::WindowProc(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam) {

#ifdef USE_IMGUI
	if (ImGui_ImplWin32_WndProcHandler(
		hwnd,
		msg,
		wParam,
		lParam)) {

		return true;
	}
#endif

	switch (msg) {
	case WM_DESTROY:

		// OSに終了を通知
		PostQuitMessage(0);

		return 0;
	}

	return DefWindowProc(
		hwnd,
		msg,
		wParam,
		lParam);
}