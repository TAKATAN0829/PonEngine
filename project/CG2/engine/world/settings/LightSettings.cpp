#include "settings/LightSettings.h"
#include <cmath>

// 設定単体の検証・比較。シーンの一覧やライト収集処理は参照しない。
bool ValidateLightSettings(const LightSettings& settings) {
	if (!std::isfinite(settings.outerAngle) || !std::isfinite(settings.innerAngle)) {
		return false;
	}
	return settings.outerAngle >= 1 && settings.outerAngle <= 179 && settings.innerAngle >= 0 && settings.innerAngle <= settings.outerAngle;
}

bool EqualLightSettings(const LightSettings& first, const LightSettings& second) {
	return first.outerAngle == second.outerAngle && first.innerAngle == second.innerAngle;
}

// UI・一覧管理・保存検証で共有するライト種別の規則。
bool LightTypes::IsLight(WorldObjectType type) {
	return type == WorldObjectType::PointLight || type == WorldObjectType::SpotLight || type == WorldObjectType::DirectionalLight;
}

uint32_t LightTypes::GetLimit(WorldObjectType type) {
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
	if (type == WorldObjectType::SpotLight) {
		return "Spot Light";
	}
	if (type == WorldObjectType::DirectionalLight) {
		return "Directional Light";
	}
	return "Point Light";
}

