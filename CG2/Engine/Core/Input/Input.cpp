#include "Input.h"

#include <cassert>
#include <cstring>

#pragma comment(lib,"dinput8.lib")
#pragma comment(lib,"dxguid.lib")


//=============================================================================================//
// 初期化

void Input::Initialize(WinApp* winApp) {

	// WinAppを記録
	winApp_ = winApp;

	HRESULT hr;

	// DirectInput初期化
	hr = DirectInput8Create(
		winApp_->GetHInstance(),
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		reinterpret_cast<void**>(directInput_.GetAddressOf ()),
		nullptr);

	assert(SUCCEEDED(hr));

	// Keyboard生成
	hr = directInput_->CreateDevice(
		GUID_SysKeyboard,
		keyboard_.GetAddressOf (),
		NULL);

	assert(SUCCEEDED(hr));

	// 入力データ形式セット
	hr = keyboard_->SetDataFormat(&c_dfDIKeyboard);

	assert(SUCCEEDED(hr));

	// 排他制御レベルセット
	hr = keyboard_->SetCooperativeLevel(
		winApp_->GetHwnd(),
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
	keyboard_->GetDeviceState(sizeof(key_), key_);
}

//=============================================================================================//
// 終了処理

void Input::Finalize() {

	// 解放はComPtrが行うので、入力の取得だけ止める
	if (keyboard_ != nullptr) {

		keyboard_->Unacquire();
	}

	// 解放順を明示(Keyboard → DirectInput)
	keyboard_.Reset();

	directInput_.Reset();
}

//=============================================================================================//
// 押している

bool Input::IsPressKey(BYTE keyNumber) const {

	return key_[keyNumber] != 0;
}

//=============================================================================================//
// 離している

bool Input::IsReleaseKey(BYTE keyNumber) const {

	return key_[keyNumber] == 0;
}

//=============================================================================================//
// 押した瞬間

bool Input::IsTriggerKey(BYTE keyNumber) const {

	return preKey_[keyNumber] == 0 &&
		key_[keyNumber] != 0;
}

//=============================================================================================//
// 離した瞬間

bool Input::IsReleaseTriggerKey(BYTE keyNumber) const {

	return preKey_[keyNumber] != 0 &&
		key_[keyNumber] == 0;
}