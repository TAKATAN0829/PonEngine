#pragma once
#include <wrl.h>
#include <Windows.h>
#include "WinApp.h"

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

class Input {
public:

	//=============================================================================================//
	// 初期化

	void Initialize(WinApp* winApp);

	//=============================================================================================//
	// 更新

	void Update();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// キー入力

	bool IsPressKey(BYTE keyNumber) const;

	bool IsReleaseKey(BYTE keyNumber) const;

	bool IsTriggerKey(BYTE keyNumber) const;

	bool IsReleaseTriggerKey(BYTE keyNumber) const;

private:

	// namespace省略
	template <class Type> using ComPtr = Microsoft::WRL::ComPtr<Type>;

	//=============================================================================================//
	// WinApp(借りているだけなので解放しない)

	WinApp* winApp_ = nullptr;

	//=============================================================================================//
	// DirectInput

	ComPtr<IDirectInput8> directInput_ = nullptr;

	//=============================================================================================//
	// Keyboard

	ComPtr<IDirectInputDevice8> keyboard_ = nullptr;

	//=============================================================================================//
	// Key

	BYTE key_[256] = {};

	BYTE preKey_[256] = {};
};