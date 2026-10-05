#pragma once
#include <wrl.h>
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

	// namespace省略
	template <class Type> using Comptr = Microsoft::WRL::ComPtr<Type>;

private:

	//=============================================================================================//
	// DirectInput

	Microsoft::WRL::ComPtr<IDirectInput8> directInput_ = nullptr;

	//=============================================================================================//
	// Keyboard

	Microsoft::WRL::ComPtr<IDirectInputDevice8> keyboard_ = nullptr;

	//=============================================================================================//
	// Key

	BYTE key_[256] = {};

	BYTE preKey_[256] = {};
};