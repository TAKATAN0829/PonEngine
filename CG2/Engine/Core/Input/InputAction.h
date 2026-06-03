#pragma once

// C++
#include <string>
#include <unordered_map>

// 自作
#include "Input.h"

class InputAction {
public:

	//=============================================================================================//
	// 初期化処理

	static void Initialize (Input* input);

	//=============================================================================================//
	// キー登録

	static void RegisterKey (const std::string& actionName,	uint8_t key);

	//=============================================================================================//
	// 入力取得

	static bool Push (const std::string& actionName);

	static bool Trigger (const std::string& actionName);

private:

	static Input* input_;

	static std::unordered_map<std::string, uint8_t> keyMap_;
};