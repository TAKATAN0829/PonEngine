#include "settings/ParticleSettings.h"
#include <array>
#include <cmath>
#include <algorithm>

namespace {
	// 位置・速度などの三成分を、同じ規則で比較する。
	bool EqualVector(const Vector3& firstVector, const Vector3& secondVector) {
		return firstVector.x == secondVector.x && firstVector.y == secondVector.y && firstVector.z == secondVector.z;
	}
	// 極端な値をファイルから入力されても、乱数生成や行列計算を破綻させない。
	bool WithinRange(const Vector3& value, float minimum, float maximum) {
		return value.x >= minimum && value.x <= maximum && value.y >= minimum && value.y <= maximum &&
			value.z >= minimum && value.z <= maximum;
	}
}

//=============================================================================================//
// 表示の評価

ParticleAppearance EvaluateParticleAppearance(const ParticleSettings& settings, const Vector4& initialColor, float normalizedAge) {
	const float age = (std::clamp)(normalizedAge,0.0f,1.0f);
	ParticleAppearance appearance;
	appearance.color = initialColor;
	if (!settings.lifetimeAppearance) {
		appearance.color.w *= 1-age;
		return appearance;
	}
	appearance.size = std::lerp(settings.startSize,settings.endSize,age);
	appearance.color = {std::lerp(initialColor.x,settings.endColor.x,age),std::lerp(initialColor.y,settings.endColor.y,age),
		std::lerp(initialColor.z,settings.endColor.z,age),std::lerp(initialColor.w,settings.endColor.w,age)};
	if (settings.fadeIn > 0) {
		appearance.color.w *= (std::min)(1.0f,age/settings.fadeIn);
	}
	return appearance;
}

//=============================================================================================//
// プリセットの作成

ParticleSettings MakeParticlePreset(ParticlePresetKind kind) {
	ParticleSettings settings;
	settings.lifetimeAppearance = true;
	settings.softParticles = true;
	settings.spread = {0.2f,0.05f,0.2f};
	settings.velocityMinimum = {-0.15f,0.6f,-0.15f};
	settings.velocityMaximum = {0.15f,1.5f,0.15f};
	settings.particleScale = {0.5f,0.5f,0.5f};
	settings.color = {1,0.75f,0.15f,0.8f};
	settings.endColor = {0.8f,0.08f,0.01f,0};
	settings.startSize = 0.5f;
	settings.endSize = 0.1f;
	settings.fadeIn = 0.1f;
	settings.lifetimeMinimum = 0.6f;
	settings.lifetimeMaximum = 1.2f;
	settings.interval = 0.05f;
	if (kind == ParticlePresetKind::Smoke) {
		settings.blendMode = kBlendModeNormal;
		settings.color = {0.25f,0.25f,0.28f,0.35f};
		settings.endColor = {0.5f,0.5f,0.53f,0};
		settings.startSize = 0.3f;
		settings.endSize = 2.5f;
		settings.fadeIn = 0.2f;
		settings.lifetimeMinimum = 2;
		settings.lifetimeMaximum = 4;
		settings.velocityMaximum.y = 0.8f;
	} else if (kind == ParticlePresetKind::Sparks) {
		settings.spread = {0.05f,0.05f,0.05f};
		settings.velocityMinimum = {-2,1,-2};
		settings.velocityMaximum = {2,4,2};
		settings.particleScale = {0.06f,0.06f,0.06f};
		settings.accelerationEnabled = true;
		settings.acceleration = {0,-5,0};
		settings.startSize = 1;
		settings.endSize = 0;
		settings.fadeIn = 0;
		settings.softDistance = 0.1f;
		settings.lifetimeMinimum = 0.3f;
		settings.lifetimeMaximum = 0.8f;
	}
	return settings;
}

//=============================================================================================//
// 設定の検証・比較

// 各軸の最小値・最大値と有限値を確認し、発生数や寿命には実行可能な上限を設ける。
bool ValidateParticleSettings(const ParticleSettings& settings) {
	const std::array<float, 25> values = {
		settings.emitterPosition.x, settings.emitterPosition.y, settings.emitterPosition.z,
		settings.spread.x, settings.spread.y, settings.spread.z,
		settings.velocityMinimum.x, settings.velocityMinimum.y, settings.velocityMinimum.z,
		settings.velocityMaximum.x, settings.velocityMaximum.y, settings.velocityMaximum.z,
		settings.particleScale.x, settings.particleScale.y, settings.particleScale.z,
		settings.color.x, settings.color.y, settings.color.z, settings.color.w,
		settings.acceleration.x, settings.acceleration.y, settings.acceleration.z,
		settings.interval, settings.lifetimeMinimum, settings.lifetimeMaximum};
	for (float value : values) {
		if (!std::isfinite(value)) {
			return false;
		}
	}
	// 発生条件
	if (settings.spawnCount < 1 || settings.spawnCount > 100) {
		return false;
	}
	if (settings.interval < 0.01f || settings.interval > 60) {
		return false;
	}
	if (!WithinRange(settings.emitterPosition, -1000, 1000)) {
		return false;
	}
	if (!WithinRange(settings.spread, 0, 100)) {
		return false;
	}

	// 移動・寿命
	if (settings.lifetimeMinimum < 0.01f || settings.lifetimeMaximum > 60 || settings.lifetimeMinimum > settings.lifetimeMaximum) {
		return false;
	}
	if (!WithinRange(settings.velocityMinimum, -100, 100) || !WithinRange(settings.velocityMaximum, -100, 100)) {
		return false;
	}
	if (settings.velocityMinimum.x > settings.velocityMaximum.x || settings.velocityMinimum.y > settings.velocityMaximum.y || settings.velocityMinimum.z > settings.velocityMaximum.z) {
		return false;
	}
	if (!WithinRange(settings.acceleration, -100, 100)) {
		return false;
	}

	// 表示・合成
	if (!std::isfinite(settings.startSize) || !std::isfinite(settings.endSize) || !std::isfinite(settings.fadeIn) || !std::isfinite(settings.softDistance)) {
		return false;
	}
	if (settings.startSize < 0.001f || settings.startSize > 100 || settings.endSize < 0 || settings.endSize > 100) {
		return false;
	}
	if (settings.fadeIn < 0 || settings.fadeIn > 1 || settings.softDistance < 0.001f || settings.softDistance > 100) {
		return false;
	}
	const float endChannels[] = {settings.endColor.x,settings.endColor.y,settings.endColor.z,settings.endColor.w};
	for (float channel : endChannels) {
		if (!std::isfinite(channel) || channel < 0 || channel > 1) {
			return false;
		}
	}
	if (!WithinRange(settings.particleScale, 0.001f, 100)) {
		return false;
	}
	if (settings.blendMode < kBlendModeNone || settings.blendMode >= kCountOfBlendMode) {
		return false;
	}
	return settings.texture == "resources/circle.png" || settings.texture == "resources/uvChecker.png" || settings.texture == "resources/white1x1.png";
}

// 保存対象の設定だけを比較し、プレビュー中の粒子の変化では未保存表示を出さない。
bool EqualParticleSettings(const ParticleSettings& firstSettings, const ParticleSettings& secondSettings) {
	if (firstSettings.lifetimeAppearance != secondSettings.lifetimeAppearance || firstSettings.softParticles != secondSettings.softParticles) {
		return false;
	}
	if (firstSettings.startSize != secondSettings.startSize || firstSettings.endSize != secondSettings.endSize || firstSettings.fadeIn != secondSettings.fadeIn || firstSettings.softDistance != secondSettings.softDistance) {
		return false;
	}
	if (firstSettings.endColor.x != secondSettings.endColor.x || firstSettings.endColor.y != secondSettings.endColor.y || firstSettings.endColor.z != secondSettings.endColor.z || firstSettings.endColor.w != secondSettings.endColor.w) {
		return false;
	}
	return EqualVector(firstSettings.emitterPosition, secondSettings.emitterPosition) &&
		EqualVector(firstSettings.spread, secondSettings.spread) &&
		EqualVector(firstSettings.velocityMinimum, secondSettings.velocityMinimum) &&
		EqualVector(firstSettings.velocityMaximum, secondSettings.velocityMaximum) &&
		EqualVector(firstSettings.particleScale, secondSettings.particleScale) &&
		EqualVector(firstSettings.acceleration, secondSettings.acceleration) &&
		firstSettings.color.x == secondSettings.color.x && firstSettings.color.y == secondSettings.color.y &&
		firstSettings.color.z == secondSettings.color.z && firstSettings.color.w == secondSettings.color.w &&
		firstSettings.spawnCount == secondSettings.spawnCount && firstSettings.interval == secondSettings.interval &&
		firstSettings.lifetimeMinimum == secondSettings.lifetimeMinimum && firstSettings.lifetimeMaximum == secondSettings.lifetimeMaximum &&
		firstSettings.randomColor == secondSettings.randomColor && firstSettings.billboard == secondSettings.billboard &&
		firstSettings.accelerationEnabled == secondSettings.accelerationEnabled &&
		firstSettings.blendMode == secondSettings.blendMode && firstSettings.texture == secondSettings.texture;
}
