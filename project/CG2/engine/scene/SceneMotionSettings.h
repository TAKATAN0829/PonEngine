#pragma once
#include "EngineStructs.h"
#include <cmath>

// 保存対象の動作Component。実行中の時計・開始位置は別のSceneMotionが所有する。
struct SceneMotionSettings {
	//=============================================================================================//
	// メンバ変数 / 回転Component

	bool hasRotation = false;
	bool rotationEnabled = true;
	Vector3 rotationDegreesPerSecond = {0, 45, 0}; // 各ローカルEuler角へ加える度/秒。

	//=============================================================================================//
	// メンバ変数 / 往復移動Component

	bool hasMovement = false;
	bool movementEnabled = true;
	Vector3 movementOffset = {0, 2, 0}; // 開始位置から終点までの、親座標系での変位。
	float movementPeriod = 2; // 開始位置→終点→開始位置の一往復にかかる秒数。
};

// 無効・取り外し中の設定も検証し、後から有効化した時の異常値を防ぐ。
inline bool ValidateMotionSettings(const SceneMotionSettings& settings) {
	const float speeds[] = {settings.rotationDegreesPerSecond.x, settings.rotationDegreesPerSecond.y, settings.rotationDegreesPerSecond.z};
	for (float speed : speeds) {
		if (!std::isfinite(speed) || std::abs(speed) > 3600) {
			return false;
		}
	}
	const float offsets[] = {settings.movementOffset.x, settings.movementOffset.y, settings.movementOffset.z};
	for (float offset : offsets) {
		if (!std::isfinite(offset) || std::abs(offset) > 100000) {
			return false;
		}
	}
	return std::isfinite(settings.movementPeriod) && settings.movementPeriod >= 0.01f && settings.movementPeriod <= 3600;
}

inline bool EqualMotionSettings(const SceneMotionSettings& first, const SceneMotionSettings& second) {
	return first.hasRotation == second.hasRotation && first.rotationEnabled == second.rotationEnabled &&
		first.rotationDegreesPerSecond.x == second.rotationDegreesPerSecond.x && first.rotationDegreesPerSecond.y == second.rotationDegreesPerSecond.y && first.rotationDegreesPerSecond.z == second.rotationDegreesPerSecond.z &&
		first.hasMovement == second.hasMovement && first.movementEnabled == second.movementEnabled &&
		first.movementOffset.x == second.movementOffset.x && first.movementOffset.y == second.movementOffset.y && first.movementOffset.z == second.movementOffset.z && first.movementPeriod == second.movementPeriod;
}
