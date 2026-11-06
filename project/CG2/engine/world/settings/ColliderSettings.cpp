#include "ColliderSettings.h"

//=============================================================================================//
// Transformに対するローカル形状

bool AreColliderSettingsValid(const ColliderSettings& settings) {
	// Colliderの種類と数値が、対応する範囲に収まるか確認する。
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
		// | size < 0.001f|厚みのない当たり判定や計算不能な値を、衝突処理に渡す前に弾く。
		if (!std::isfinite(size) || size < 0.001f || size > 100000) {
			return false;
		}
	}

	return true;
}

bool AreColliderSettingsEqual(const ColliderSettings& first, const ColliderSettings& second) {
	// Colliderの保存対象の値を比較する。
	if (first.isPresent == second.isPresent && first.isEnabled == second.isEnabled && first.isTrigger == second.isTrigger && first.shape == second.shape &&
		first.center.x == second.center.x && first.center.y == second.center.y && first.center.z == second.center.z &&
		first.size.x == second.size.x && first.size.y == second.size.y && first.size.z == second.size.z && first.radius == second.radius) {
		return true;
	}

	return false;
}
