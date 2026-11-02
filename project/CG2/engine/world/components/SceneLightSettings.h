#pragma once
#include "LightLimits.h"
#include "SceneObjectTypes.h"

// Spotの角度は円錐全体の開き角（度）。位置・向きはTransform、共通の色・強度・範囲はObjectが持つ。
struct SceneLightSettings {
	float outerAngle = 60; // 円錐の外側。この角度の外は照らさない（1〜179度）。
	float innerAngle = 0; // 減衰しない中心域。外角以下に保つ。同角度なら硬い境界。
};

bool ValidateLightSettings(const SceneLightSettings& settings);
bool EqualLightSettings(const SceneLightSettings& first, const SceneLightSettings& second);

// ライト分類と作成上限。具体的なシーンや実行Systemを参照せず、共通規則だけを公開する。
namespace SceneLightTypes {
	bool IsLight(SceneObjectType type);
	uint32_t GetLimit(SceneObjectType type);
	const char* GetName(SceneObjectType type);
}
