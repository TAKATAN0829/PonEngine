#include "settings/ParticleSettings.h"
#include <array>
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <cctype>

//=============================================================================================//
// 初期化処理

ParticleSettings::ParticleSettings()
	: emitterPosition{},
	spread{1,1,1},
	spawnCount(3),
	interval(0.1f),
	velocityMinimum{-1,-1,-1},
	velocityMaximum{1,1,1},
	acceleration{0,1,0},
	isAccelerationEnabled(false),
	lifetimeMinimum(1),
	lifetimeMaximum(3),
	particleScale{1,1,1},
	color{1,0.65f,0.2f,1},
	isRandomColorEnabled(false),
	isBillboardEnabled(true),
	blendMode(kBlendModeAdd),
	texture("resources/circle.png"),
	isLifetimeAppearanceEnabled(false),
	startSize(1),
	endSize(1),
	endColor{1,0.65f,0.2f,0},
	fadeIn(0),
	areSoftParticlesEnabled(false),
	softDistance(0.5f) {
	// 新しいEmitterとプリセットが共通で使う、発生・速度・外観の既定値を設定する。
}

ParticlePreviewState::ParticlePreviewState()
	: targetObject(0),
	isEnabled(false),
	isPaused(false),
	isStepRequested(false),
	shouldIsolateParticles(false),
	isAutomaticEmissionEnabled(true) {
	// プレビューを停止した状態から始め、開始後は自動発生を許可する。
}

namespace {
	// 位置・速度などの三成分を、同じ規則で比較する。
	bool AreVectorsEqual(const Vector3& firstVector, const Vector3& secondVector) {
		// 三つの成分がすべて一致するか確認する。
		return firstVector.x == secondVector.x && firstVector.y == secondVector.y && firstVector.z == secondVector.z;
	}

	// 極端な値をファイルから入力されても、乱数生成や行列計算を破綻させない。
	bool IsWithinRange(const Vector3& value, float minimum, float maximum) {
		// 有限値で、指定した最小値と最大値の間にあるか確認する。
		return value.x >= minimum && value.x <= maximum && value.y >= minimum && value.y <= maximum &&
			value.z >= minimum && value.z <= maximum;
	}
}

//=============================================================================================//
// 表示の評価

ParticleAppearance EvaluateParticleAppearance(const ParticleSettings& settings, const Vector4& initialColor, float normalizedAge) {
	// 寿命の進み具合から、現在の色と大きさを求める。
	const float age = (std::clamp)(normalizedAge,0.0f,1.0f);

	// 寿命による拡縮を使わない場合の大きさと色を、生成時に指定する。
	ParticleAppearance appearance{1, initialColor};

	if (!settings.isLifetimeAppearanceEnabled) {
		appearance.color.w *= 1-age;

		return appearance;
	}

	// 寿命を0〜1にした進み具合で、開始時と終了時の大きさ・色を補間する。
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
	// 種類に応じたParticleの初期設定を組み立てる。
	ParticleSettings settings;

	settings.isLifetimeAppearanceEnabled = true;

	settings.areSoftParticlesEnabled = true;

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

		settings.isAccelerationEnabled = true;

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
bool AreParticleSettingsValid(const ParticleSettings& settings) {
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

	if (!IsWithinRange(settings.emitterPosition, -1000, 1000)) {
		return false;
	}

	if (!IsWithinRange(settings.spread, 0, 100)) {
		return false;
	}

	// 移動・寿命
	if (settings.lifetimeMinimum < 0.01f || settings.lifetimeMaximum > 60 || settings.lifetimeMinimum > settings.lifetimeMaximum) {
		return false;
	}

	if (!IsWithinRange(settings.velocityMinimum, -100, 100) || !IsWithinRange(settings.velocityMaximum, -100, 100)) {
		return false;
	}

	if (settings.velocityMinimum.x > settings.velocityMaximum.x || settings.velocityMinimum.y > settings.velocityMaximum.y || settings.velocityMinimum.z > settings.velocityMaximum.z) {
		return false;
	}

	if (!IsWithinRange(settings.acceleration, -100, 100)) {
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

	if (!IsWithinRange(settings.particleScale, 0.001f, 100)) {
		return false;
	}

	if (settings.blendMode < kBlendModeNone || settings.blendMode >= kCountOfBlendMode) {
		return false;
	}

	if (settings.texture.empty() || settings.texture.size() > 255 || settings.texture.find('\0') != std::string::npos) {
		return false;
	}

	const std::filesystem::path texture(std::u8string(settings.texture.begin(), settings.texture.end()));

	if (texture.is_absolute() || texture.has_root_name()) {
		return false;
	}

	if (*texture.begin() != "resources") {
		return false;
	}

	for (const std::filesystem::path& part : texture) {
		if (part == "..") {
			return false;
		}
	}

	std::string extension = texture.extension().string();

	for (char& character : extension) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	return extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp";
}

// 保存対象の設定だけを比較し、プレビュー中の粒子の変化では未保存表示を出さない。
bool AreParticleSettingsEqual(const ParticleSettings& firstSettings, const ParticleSettings& secondSettings) {
	// Particleの保存対象の値を比較する。
	if (firstSettings.isLifetimeAppearanceEnabled != secondSettings.isLifetimeAppearanceEnabled || firstSettings.areSoftParticlesEnabled != secondSettings.areSoftParticlesEnabled) {
		return false;
	}

	if (firstSettings.startSize != secondSettings.startSize || firstSettings.endSize != secondSettings.endSize || firstSettings.fadeIn != secondSettings.fadeIn || firstSettings.softDistance != secondSettings.softDistance) {
		return false;
	}

	if (firstSettings.endColor.x != secondSettings.endColor.x || firstSettings.endColor.y != secondSettings.endColor.y || firstSettings.endColor.z != secondSettings.endColor.z || firstSettings.endColor.w != secondSettings.endColor.w) {
		return false;
	}

	return AreVectorsEqual(firstSettings.emitterPosition, secondSettings.emitterPosition) &&
		AreVectorsEqual(firstSettings.spread, secondSettings.spread) &&
		AreVectorsEqual(firstSettings.velocityMinimum, secondSettings.velocityMinimum) &&
		AreVectorsEqual(firstSettings.velocityMaximum, secondSettings.velocityMaximum) &&
		AreVectorsEqual(firstSettings.particleScale, secondSettings.particleScale) &&
		AreVectorsEqual(firstSettings.acceleration, secondSettings.acceleration) &&
		firstSettings.color.x == secondSettings.color.x && firstSettings.color.y == secondSettings.color.y &&
		firstSettings.color.z == secondSettings.color.z && firstSettings.color.w == secondSettings.color.w &&
		firstSettings.spawnCount == secondSettings.spawnCount && firstSettings.interval == secondSettings.interval &&
		firstSettings.lifetimeMinimum == secondSettings.lifetimeMinimum && firstSettings.lifetimeMaximum == secondSettings.lifetimeMaximum &&
		firstSettings.isRandomColorEnabled == secondSettings.isRandomColorEnabled && firstSettings.isBillboardEnabled == secondSettings.isBillboardEnabled &&
		firstSettings.isAccelerationEnabled == secondSettings.isAccelerationEnabled &&
		firstSettings.blendMode == secondSettings.blendMode && firstSettings.texture == secondSettings.texture;
}
