#pragma once

#include <Windows.h>
#include <cstdint>

#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

class Input {
public:

	//=============================================================================================//
	// 初期化

	void Initialize(HINSTANCE hInstance, HWND hwnd);

	//=============================================================================================//
	// 更新

	void Update();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// キー入力

	bool IsPressKey(uint8_t keyNumber) const;

	bool IsReleaseKey(uint8_t keyNumber) const;

	bool IsTriggerKey(uint8_t keyNumber) const;

	bool IsReleaseTriggerKey(uint8_t keyNumber) const;

private:

	//=============================================================================================//
	// DirectInput

	IDirectInput8* directInput_ = nullptr;

	//=============================================================================================//
	// Keyboard

	IDirectInputDevice8* keyboard_ = nullptr;

	//=============================================================================================//
	// Key

	BYTE key_[256] = {};

	BYTE preKey_[256] = {};
};