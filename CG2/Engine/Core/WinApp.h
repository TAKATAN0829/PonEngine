#pragma once

#include <Windows.h>
#include <fstream>
#include <cstdint>

class WinApp {
public:

	//=============================================================================================//
	// 初期化

	void Initialize();

	//=============================================================================================//
	// メッセージ処理

	bool ProcessMessage();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Getter

	HWND GetHwnd() const {
		return hwnd_;
	}

	int32_t GetClientWidth() const {
		return kClientWidth_;
	}

	int32_t GetClientHeight() const {
		return kClientHeight_;
	}

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
	// WindowSize

	static const int32_t kClientWidth_ = 1280;
	static const int32_t kClientHeight_ = 720;

	//=============================================================================================//
	// Window

	HWND hwnd_ = nullptr;

	//=============================================================================================//
	// WindowClass

	WNDCLASS wc_{};

	//=============================================================================================//
	// Message

	MSG msg_{};
};