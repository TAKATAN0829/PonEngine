#pragma once

#include <Windows.h>
#include <cstdint>

class WinApp {
public:

	//=============================================================================================//
	// 初期化

	void Initialize();

	//=============================================================================================//
	// 更新

	void Update();

	//=============================================================================================//
	// メッセージの処理

	bool ProcessMessage();
	bool IsCloseRequested() const { return closeRequested_; }
	void ClearCloseRequest() { closeRequested_ = false; }
	void ConfirmClose();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Getter

	HWND GetHwnd() const { return hwnd_; }

	HINSTANCE GetHInstance() const { return wc_.hInstance; }

	//=============================================================================================//
	// WindowSize

	static const int32_t kClientWidth = 1600;
	static const int32_t kClientHeight = 900;

private:

	//=============================================================================================//
	// ウィンドウプロシージャ

	static LRESULT CALLBACK WindowProc(
		HWND hwnd,
		UINT msg,
		WPARAM wParam,
		LPARAM lParam);

private:
	//=============================================================================================//
	// Window

	HWND hwnd_ = nullptr;
	bool closeRequested_ = false;

	//=============================================================================================//
	// WindowClass

	WNDCLASS wc_{};
};
