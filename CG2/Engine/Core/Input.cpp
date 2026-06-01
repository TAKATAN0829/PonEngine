#include "Input.h"

#include <cassert>
#include <cstring>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")

//=============================================================================================//
// 初期化

void Input::Initialize(HINSTANCE hInstance,	HWND hwnd) {

	HRESULT hr;

	// DirectInput初期化
	hr = DirectInput8Create(
		hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void**>(&directInput_),
		nullptr);

	assert(SUCCEEDED(hr));

	// Keyboard生成
	hr = directInput_->CreateDevice(
		GUID_SysKeyboard,
		&keyboard_,
		NULL);

	assert(SUCCEEDED(hr));

	// 入力データ形式セット
	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);

	assert(SUCCEEDED(hr));

	// 排他制御レベルセット
	hr = keyboard_->SetCooperativeLevel(
		hwnd,
		DISCL_FOREGROUND |
		DISCL_NONEXCLUSIVE |
		DISCL_NOWINKEY);

	assert(SUCCEEDED(hr));
}

//=============================================================================================//
// 更新

void Input::Update() {

	// 前フレームの入力を保存
	memcpy(preKey_, key_, sizeof(key_));

	// キーボード取得開始
	keyboard_->Acquire();

	// キー状態取得
	keyboard_->GetDeviceState(
		sizeof(key_),
		key_);
}

//=============================================================================================//
// 終了処理

void Input::Finalize() {

	if (keyboard_ != nullptr) {

		keyboard_->Unacquire();

		keyboard_->Release();

		keyboard_ = nullptr;
	}

	if (directInput_ != nullptr) {

		directInput_->Release();

		directInput_ = nullptr;
	}
}

//=============================================================================================//
// 押している

bool Input::IsPressKey(uint8_t keyNumber) const {

	return key_[keyNumber] != 0;
}

//=============================================================================================//
// 離している

bool Input::IsReleaseKey(uint8_t keyNumber) const {

	return key_[keyNumber] == 0;
}

//=============================================================================================//
// 押した瞬間

bool Input::IsTriggerKey(uint8_t keyNumber) const {

	return preKey_[keyNumber] == 0 &&
		key_[keyNumber] != 0;
}

//=============================================================================================//
// 離した瞬間

bool Input::IsReleaseTriggerKey(uint8_t keyNumber) const {

	return preKey_[keyNumber] != 0 &&
		key_[keyNumber] == 0;
}