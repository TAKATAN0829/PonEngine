#pragma once

// Spotの角度は円錐全体の開き角（度）。位置・向きはTransform、共通の色・強度・範囲はObjectが持つ。
struct SceneLightSettings {
	float outerAngle = 60; // 円錐の外側。この角度の外は照らさない（1〜179度）。
	float innerAngle = 0; // 減衰しない中心域。外角以下に保つ。同角度なら硬い境界。
};

bool ValidateLightSettings(const SceneLightSettings& settings);
bool EqualLightSettings(const SceneLightSettings& first, const SceneLightSettings& second);
