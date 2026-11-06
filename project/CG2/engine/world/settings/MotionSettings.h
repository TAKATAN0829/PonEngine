#pragma once

#include "EngineStructs.h"
#include <cmath>

// 保存対象の動作Component。実行中の時計・開始位置は別のWorldMotionが所有する。
struct MotionSettings {
	MotionSettings();

	//=============================================================================================//
	// 回転Component

	bool hasRotation;
	bool isRotationEnabled;

	Vector3 rotationDegreesPerSecond; // 各ローカルEuler角へ加える度/秒。

	//=============================================================================================//
	// 往復移動Component

	bool hasMovement;
	bool isMovementEnabled;

	Vector3 movementOffset; // 開始位置から終点までの、親座標系での変位。

	float movementPeriod; // 開始位置→終点→開始位置の一往復にかかる秒数。
};

// 無効・取り外し中の設定も検証し、後から有効化した時の異常値を防ぐ。
bool AreMotionSettingsValid(const MotionSettings& settings);

bool AreMotionSettingsEqual(const MotionSettings& first, const MotionSettings& second);
