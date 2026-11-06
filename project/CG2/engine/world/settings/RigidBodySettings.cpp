#include "RigidBodySettings.h"

//=============================================================================================//
// 質量・接触特性・初速

bool AreBodySettingsValid(const RigidBodySettings& settings) {
	// Rigidbodyの種類と数値が、対応する範囲に収まるか確認する。
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

	// 反発や摩擦などの係数を、物理計算で扱う0〜1の範囲にそろえる。
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

bool AreBodySettingsEqual(const RigidBodySettings& first, const RigidBodySettings& second) {
	// Rigidbodyの保存対象の値を比較する。
	if (first.isPresent == second.isPresent && first.isEnabled == second.isEnabled && first.mode == second.mode && first.isGravityEnabled == second.isGravityEnabled &&
		first.mass == second.mass && first.gravityScale == second.gravityScale && first.restitution == second.restitution && first.friction == second.friction && first.linearDamping == second.linearDamping &&
		first.initialVelocity.x == second.initialVelocity.x && first.initialVelocity.y == second.initialVelocity.y && first.initialVelocity.z == second.initialVelocity.z) {
		return true;
	}

	return false;
}
