#include "MotionSettings.h"

//=============================================================================================//
// 初期化処理

MotionSettings::MotionSettings()
	: hasRotation(false),
	isRotationEnabled(true),
	rotationDegreesPerSecond{0, 45, 0},
	hasMovement(false),
	isMovementEnabled(true),
	movementOffset{0, 2, 0},
	movementPeriod(2) {
	// 追加前の動作に、Y軸回転と上下の往復移動の設定を用意する。
}

//=============================================================================================//
// 往復移動Component

// 無効・取り外し中の設定も検証し、後から有効化した時の異常値を防ぐ。
bool AreMotionSettingsValid(const MotionSettings& settings) {
	// 回転と往復移動の種類と数値が、対応する範囲に収まるか確認する。

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

	// 往復の周期は秒で扱う。後で周期で割って進み具合を求めるため、0を許可しない。
	return std::isfinite(settings.movementPeriod) && settings.movementPeriod >= 0.01f && settings.movementPeriod <= 3600;
}

bool AreMotionSettingsEqual(const MotionSettings& first, const MotionSettings& second) {
	// 回転と往復移動の保存対象の値を比較する。
	if (first.hasRotation == second.hasRotation && first.isRotationEnabled == second.isRotationEnabled &&
		first.rotationDegreesPerSecond.x == second.rotationDegreesPerSecond.x && first.rotationDegreesPerSecond.y == second.rotationDegreesPerSecond.y && first.rotationDegreesPerSecond.z == second.rotationDegreesPerSecond.z &&
		first.hasMovement == second.hasMovement && first.isMovementEnabled == second.isMovementEnabled &&
		first.movementOffset.x == second.movementOffset.x && first.movementOffset.y == second.movementOffset.y && first.movementOffset.z == second.movementOffset.z && first.movementPeriod == second.movementPeriod) {
		return true;
	}

	return false;
}
