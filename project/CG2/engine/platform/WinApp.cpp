#include "WinApp.h"

#pragma comment(lib, "winmm.lib")

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

//=============================================================================================//
// 実装内補助定義

// Win32入力イベントを処理。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam);

#endif

//=============================================================================================//
// 初期化


void WinApp::Initialize() {
	hasQuitMessage_ = false;

	// システムタイマーの分解能を上げる
	timeBeginPeriod(1);

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
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

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
		this);

	// Window表示
	ShowWindow(hwnd_, SW_SHOW);
}

//=============================================================================================//
// 更新


void WinApp::Update() {
	// 現時点ではフレームごとの追加処理を行わない。

}

//=============================================================================================//
// メッセージの処理


void WinApp::ProcessMessage() {

	MSG message{};

	// マウス移動とキー入力を同時に行っても、古い入力を次フレームへ溜めない。
	// メッセージが来るまで待たず、今ある分だけ処理してゲームの更新へ戻る。
	while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
		if (message.message == WM_QUIT) {
			hasQuitMessage_ = true;
			return;
		}

		TranslateMessage(&message);

		DispatchMessage(&message);
	}
}

//=============================================================================================//
// 終了処理


void WinApp::Finalize() {
	// Windowを閉じる
	CloseWindow(hwnd_);

	CoUninitialize();
}

//=============================================================================================//
// Getter

bool WinApp::IsCloseRequested() const {
	// 保存確認を待っているウィンドウ終了要求があるか確認する。
	return isCloseRequested_;
}

bool WinApp::HasQuitMessage() const {
	return hasQuitMessage_;
}

HWND WinApp::GetHwnd() const {
	// 生成したウィンドウのハンドルを取得する。
	return hwnd_;
}

HINSTANCE WinApp::GetHInstance() const {
	// 起動時に受け取ったアプリのインスタンスハンドルを取得する。
	return wc_.hInstance;
}

//=============================================================================================//
// Setter / 終了要求

void WinApp::ClearCloseRequest() {
	// 確認待ちの終了要求を取り消す。
	isCloseRequested_ = false;
}

// 保存確認が済んだ後にウィンドウを破棄し、通常の終了メッセージを発生させる。
void WinApp::ConfirmClose() {
	// 終了確認が完了したことをウィンドウへ通知する。
	DestroyWindow(hwnd_);
}

//=============================================================================================//
// ウィンドウプロシージャ


LRESULT CALLBACK WinApp::WindowProc(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam) {
	if (msg == WM_NCCREATE) {
		// OSのウィンドウとWinAppを対応付け、後続のメッセージから所有者を取得できるようにする。
		const CREATESTRUCT* creationData = reinterpret_cast<const CREATESTRUCT*>(lParam);

		SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(creationData->lpCreateParams));
	}

#ifdef USE_IMGUI
	// Win32入力イベントを処理。
	if (ImGui_ImplWin32_WndProcHandler(
		hwnd,
		msg,
		wParam,
		lParam)) {

		return true;
	}

#endif

	switch (msg) {
		case WM_CLOSE: {
			// ここでは破棄せず、Editorが未保存の変更を確認できるように終了要求だけを残す。
			WinApp* window = reinterpret_cast<WinApp*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));

			if (window != nullptr) {
				window->isCloseRequested_ = true;

				return 0;
			}

			break;
		}

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
