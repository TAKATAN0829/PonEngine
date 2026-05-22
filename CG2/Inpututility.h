#pragma once
#pragma once
#include <Windows.h>
#include <cstdint>

namespace InputUtility {

	inline bool IsPressKey(BYTE key[256], uint8_t keyNumber) {
		return key[keyNumber] != 0;
	}

	inline bool IsReleaseKey(BYTE key[256], uint8_t keyNumber) {
		return key[keyNumber] == 0;
	}

	inline bool IsTriggerKey(BYTE key[256], BYTE preKey[256], uint8_t keyNumber) {
		return preKey[keyNumber] == 0 && key[keyNumber] != 0;
	}

	inline bool IsReleaseTriggerKey(BYTE key[256], BYTE preKey[256], uint8_t keyNumber) {
		return preKey[keyNumber] != 0 && key[keyNumber] == 0;
	}
}