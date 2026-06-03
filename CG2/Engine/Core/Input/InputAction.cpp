#include "InputAction.h"

Input* InputAction::input_ = nullptr;

std::unordered_map<std::string, uint8_t> InputAction::keyMap_;

//=============================================================================================//
// 初期化処理

void InputAction::Initialize (Input* input) {

	input_ = input;
}

//=============================================================================================//
// キー登録

void InputAction::RegisterKey (
	const std::string& actionName,
	uint8_t key) {

	keyMap_[actionName] = key;
}

//=============================================================================================//
// 入力取得

bool InputAction::Push (const std::string& actionName) {

	if (!input_) {
		return false;
	}

	if (keyMap_.find (actionName) == keyMap_.end ()) {
		return false;
	}

	return input_->IsPressKey (keyMap_[actionName]);
}

bool InputAction::Trigger (const std::string& actionName) {

	if (!input_) {
		return false;
	}

	if (keyMap_.find (actionName) == keyMap_.end ()) {
		return false;
	}

	return input_->IsTriggerKey (keyMap_[actionName]);
}