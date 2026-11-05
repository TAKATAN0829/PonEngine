#include "AssetSettings.h"

//=============================================================================================//
// 共通処理

// 設定の比較はUIと履歴で共通にし、見た目の変更をUndoから漏らさない。
bool AreMaterialSettingsEqual(const MaterialSettings& first, const MaterialSettings& second) {
	// Materialの保存対象の値を比較する。
	if (first.slot == second.slot && first.texture == second.texture && first.color.x == second.color.x &&
		first.color.y == second.color.y && first.color.z == second.color.z && first.color.w == second.color.w &&
		first.shininess == second.shininess && first.isLightingEnabled == second.isLightingEnabled) {
		return true;
	}

	return false;
}

bool AreAnimationSettingsEqual(const AnimationSettings& first, const AnimationSettings& second) {
	// アニメーションの保存対象の値を比較する。
	if (first.clip == second.clip && first.shouldAutoplay == second.shouldAutoplay && first.shouldLoop == second.shouldLoop &&
		first.speed == second.speed && first.time == second.time) {
		return true;
	}

	return false;
}
