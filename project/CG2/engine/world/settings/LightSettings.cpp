#include "settings/LightSettings.h"
#include <cmath>

//=============================================================================================//
// 設定の検証・比較

// 設定単体の検証・比較。シーンの一覧やライト収集処理は参照しない。
bool AreLightSettingsValid(const LightSettings& settings) {
	// ライトの種類と数値が、対応する範囲に収まるか確認する。
	if (!std::isfinite(settings.outerAngle) || !std::isfinite(settings.innerAngle)) {
		return false;
	}

	// 内側の光の範囲を外側の円錐に収め、スポットライトの角度を有効な範囲に制限する。
	return settings.outerAngle >= 1 && settings.outerAngle <= 179 && settings.innerAngle >= 0 && settings.innerAngle <= settings.outerAngle;
}

bool AreLightSettingsEqual(const LightSettings& first, const LightSettings& second) {
	// ライトの保存対象の値を比較する。
	return first.outerAngle == second.outerAngle && first.innerAngle == second.innerAngle;
}

//=============================================================================================//
// ライト種別

// UI・一覧管理・保存検証で共有するライト種別の規則。
bool LightTypes::IsLight(WorldObjectType type) {
	// オブジェクトの種類がライトに該当するか確認する。
	return type == WorldObjectType::PointLight || type == WorldObjectType::SpotLight || type == WorldObjectType::DirectionalLight;
}

uint32_t LightTypes::GetLimit(WorldObjectType type) {
	// オブジェクトの種類に対応する作成上限を返す。
	if (type == WorldObjectType::PointLight) {
		return kMaxPointLights;
	}

	if (type == WorldObjectType::SpotLight) {
		return kMaxSpotLights;
	}

	if (type == WorldObjectType::DirectionalLight) {
		return kMaxDirectionalLights;
	}

	return 0;
}

const char* LightTypes::GetName(WorldObjectType type) {
	// 種類や場面を表す表示名を取得する。
	if (type == WorldObjectType::SpotLight) {
		return "Spot Light";
	}

	if (type == WorldObjectType::DirectionalLight) {
		return "Directional Light";
	}

	return "Point Light";
}

