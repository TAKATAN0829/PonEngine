#include "ParticleSettings.h"
#include <array>
#include <cmath>

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
