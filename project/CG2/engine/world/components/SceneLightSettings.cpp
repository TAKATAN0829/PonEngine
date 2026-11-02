#include "components/SceneLightSettings.h"
#include <cmath>

// 設定単体の検証・比較。シーンの一覧やライト収集処理は参照しない。
bool ValidateLightSettings(const SceneLightSettings& settings) {
	if (!std::isfinite(settings.outerAngle) || !std::isfinite(settings.innerAngle)) {
		return false;
	}
	return settings.outerAngle >= 1 && settings.outerAngle <= 179 && settings.innerAngle >= 0 && settings.innerAngle <= settings.outerAngle;
}

bool EqualLightSettings(const SceneLightSettings& first, const SceneLightSettings& second) {
	return first.outerAngle == second.outerAngle && first.innerAngle == second.innerAngle;
}

// UI・一覧管理・保存検証で共有するライト種別の規則。
bool SceneLightTypes::IsLight(SceneObjectType type) {
	return type == SceneObjectType::PointLight || type == SceneObjectType::SpotLight || type == SceneObjectType::DirectionalLight;
}

uint32_t SceneLightTypes::GetLimit(SceneObjectType type) {
	if (type == SceneObjectType::PointLight) {
		return kMaxPointLights;
	}
	if (type == SceneObjectType::SpotLight) {
		return kMaxSpotLights;
	}
	if (type == SceneObjectType::DirectionalLight) {
		return kMaxDirectionalLights;
	}
	return 0;
}

const char* SceneLightTypes::GetName(SceneObjectType type) {
	if (type == SceneObjectType::SpotLight) {
		return "Spot Light";
	}
	if (type == SceneObjectType::DirectionalLight) {
		return "Directional Light";
	}
	return "Point Light";
}

