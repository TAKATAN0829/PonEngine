#pragma once

#include "EngineStructs.h"
#include <cmath>
#include <cstddef>

inline constexpr size_t kMaxWorldColliders = 256;

enum class ColliderShape { Box, Sphere };

// 一つのオブジェクトに一つの判定形状。押し戻し・重力の設定はRigidbodyが所有する。
struct ColliderSettings {
	ColliderSettings();

	//=============================================================================================//
	// 存在・有効状態

	bool isPresent;
	bool isEnabled;
	bool isTrigger; // 通知だけを行い、押し戻しを適用しない。

	ColliderShape shape;

	//=============================================================================================//
	// Transformに対するローカル形状

	Vector3 center;
	Vector3 size; // Boxの全幅。親子の回転・拡縮も反映する。

	float radius; // Sphereはワールド変換後の外接球で判定する。
};

bool AreColliderSettingsValid(const ColliderSettings& settings);

bool AreColliderSettingsEqual(const ColliderSettings& first, const ColliderSettings& second);
