#pragma once
#include "EngineStructs.h"
#include <cmath>

enum class BodyMode { Static, Dynamic, Kinematic };

// 並進運動用のRigidbody設定。実行中の速度はScenePhysicsが別に所有する。
struct SceneBodySettings {
	//=============================================================================================//
	// メンバ変数 / 存在・動作方式

	bool present = false; // 有効な親RigidbodyもないColliderは固定物として扱う。
	bool enabled = true;
	BodyMode mode = BodyMode::Dynamic;
	bool useGravity = true;

	//=============================================================================================//
	// メンバ変数 / 質量・接触特性・初速

	float mass = 1; // kg。Dynamic同士の押し戻し・速度変化を質量で配分する。
	float gravityScale = 1;
	float restitution = 0; // 0は跳ね返らず、1は弾性衝突。
	float friction = 0.6f;
	float linearDamping = 0;
	Vector3 initialVelocity{}; // ワールド単位/秒。Play開始時だけ適用する。
};

inline bool ValidateBodySettings(const SceneBodySettings& settings) {
	if (settings.mode != BodyMode::Static && settings.mode != BodyMode::Dynamic && settings.mode != BodyMode::Kinematic) {
		return false;
	}
	if (!std::isfinite(settings.mass) || settings.mass < 0.001f || settings.mass > 100000) {
		return false;
	}
	if (!std::isfinite(settings.gravityScale) || settings.gravityScale < 0 || settings.gravityScale > 10) {
		return false;
	}
	const float coefficients[] = {settings.restitution, settings.friction};
	for (float coefficient : coefficients) {
		if (!std::isfinite(coefficient) || coefficient < 0 || coefficient > 1) {
			return false;
		}
	}
	if (!std::isfinite(settings.linearDamping) || settings.linearDamping < 0 || settings.linearDamping > 100) {
		return false;
	}
	const float velocities[] = {settings.initialVelocity.x, settings.initialVelocity.y, settings.initialVelocity.z};
	for (float velocity : velocities) {
		if (!std::isfinite(velocity) || std::abs(velocity) > 1000) {
			return false;
		}
	}
	return true;
}

inline bool EqualBodySettings(const SceneBodySettings& first, const SceneBodySettings& second) {
	return first.present == second.present && first.enabled == second.enabled && first.mode == second.mode && first.useGravity == second.useGravity &&
		first.mass == second.mass && first.gravityScale == second.gravityScale && first.restitution == second.restitution && first.friction == second.friction && first.linearDamping == second.linearDamping &&
		first.initialVelocity.x == second.initialVelocity.x && first.initialVelocity.y == second.initialVelocity.y && first.initialVelocity.z == second.initialVelocity.z;
}
