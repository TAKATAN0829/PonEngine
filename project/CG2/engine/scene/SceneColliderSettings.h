#pragma once
#include "EngineStructs.h"
#include <cmath>
#include <cstddef>

inline constexpr size_t kMaxSceneColliders = 256;

enum class ColliderShape { Box, Sphere };

// 一つのオブジェクトに一つの判定形状。押し戻し・重力の設定はRigidbodyが所有する。
struct SceneColliderSettings {
	//=============================================================================================//
	// メンバ変数 / 存在・有効状態

	bool present = false;
	bool enabled = true;
	bool trigger = false; // 通知だけを行い、押し戻しを適用しない。
	ColliderShape shape = ColliderShape::Box;

	//=============================================================================================//
	// メンバ変数 / Transformに対するローカル形状

	Vector3 center{};
	Vector3 size = {1,1,1}; // Boxの全幅。親子の回転・拡縮も反映する。
	float radius = 0.5f; // Sphereはワールド変換後の外接球で判定する。
};

inline bool ValidateColliderSettings(const SceneColliderSettings& settings) {
	if (settings.shape != ColliderShape::Box && settings.shape != ColliderShape::Sphere) {
		return false;
	}
	const float centers[] = {settings.center.x, settings.center.y, settings.center.z};
	for (float center : centers) {
		if (!std::isfinite(center) || std::abs(center) > 100000) {
			return false;
		}
	}
	const float sizes[] = {settings.size.x, settings.size.y, settings.size.z, settings.radius};
	for (float size : sizes) {
		if (!std::isfinite(size) || size < 0.001f || size > 100000) {
			return false;
		}
	}
	return true;
}

inline bool EqualColliderSettings(const SceneColliderSettings& first, const SceneColliderSettings& second) {
	return first.present == second.present && first.enabled == second.enabled && first.trigger == second.trigger && first.shape == second.shape &&
		first.center.x == second.center.x && first.center.y == second.center.y && first.center.z == second.center.z &&
		first.size.x == second.size.x && first.size.y == second.size.y && first.size.z == second.size.z && first.radius == second.radius;
}
