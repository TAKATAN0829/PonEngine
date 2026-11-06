#pragma once

// 自作
#include "EngineStructs.h"
#include "BlendMode.h"

// C++
#include <string>

// 保存・履歴用の設定。実行中の粒子やGPUリソースは含めない。
struct ParticleSettings {
	ParticleSettings();

	//=============================================================================================//
	// 発生条件

	Vector3 emitterPosition; // 個別EmitterはTransformの位置を使う。
	Vector3 spread; // 発生位置をランダムに広げる範囲。

	uint32_t spawnCount; // 一回の発生数。最大100。

	float interval; // 発生間隔（秒）。

	//=============================================================================================//
	// 移動・寿命

	Vector3 velocityMinimum;
	Vector3 velocityMaximum;
	Vector3 acceleration;

	bool isAccelerationEnabled;

	float lifetimeMinimum; // 最短寿命（秒）。
	float lifetimeMaximum; // 最長寿命（秒）。

	//=============================================================================================//
	// 表示・合成

	Vector3 particleScale;

	Vector4 color;

	bool isRandomColorEnabled; // 設定色を基準に粒子ごとの色を変える。
	bool isBillboardEnabled; // 各粒子をカメラの方へ向ける。

	BlendMode blendMode;

	std::string texture;

	//=============================================================================================//
	// 寿命による見た目 / 既存データは無効で従来のAlpha減衰を維持する。

	bool isLifetimeAppearanceEnabled;

	float startSize; // particleScaleに掛ける発生時の倍率。
	float endSize; // 消滅時の倍率。0なら縮んで消える。

	Vector4 endColor;

	float fadeIn; // 寿命に対するフェードイン区間（0〜1）。

	//=============================================================================================//
	// モデルとの交差 / ワールド単位で境界をぼかす。

	bool areSoftParticlesEnabled;

	float softDistance;
};

enum class ParticlePresetKind { 
	Fire,
	Smoke, 
	Sparks 
};

// 実行状態を持たない評価値。CPUテストと実描画で同じ補間を使う。
struct ParticleAppearance {
	float size;

	Vector4 color;
};

//=============================================================================================//
// 表示の評価

ParticleAppearance EvaluateParticleAppearance(const ParticleSettings& settings, const Vector4& initialColor, float normalizedAge);

//=============================================================================================//
// プリセットの作成

ParticleSettings MakeParticlePreset(ParticlePresetKind kind);

// 操作中だけの状態。プリセットとシーンファイルの保存対象には含めない。
struct ParticlePreviewState {
	ParticlePreviewState();

	//=============================================================================================//
	// 対象・再生状態

	uint64_t targetObject; // 0は共通プリセット、それ以外はシーン内EmitterのID。

	bool isEnabled;
	bool isPaused;
	bool isStepRequested; // 次の一回だけ時間を進める。

	//=============================================================================================//
	// 表示・発生モード

	bool shouldIsolateParticles; // モデルを隠してParticleだけを確認する。
	bool isAutomaticEmissionEnabled; // falseならBurst操作だけで発生する。
};

//=============================================================================================//
// 設定の検証・比較

// 実行前・保存前・読込後に同じ規則で数値とアセット参照を検証する。
bool AreParticleSettingsValid(const ParticleSettings& settings);

// 実行中の粒子は比較せず、保存される設定だけを比較する。
bool AreParticleSettingsEqual(const ParticleSettings& firstSettings, const ParticleSettings& secondSettings);
