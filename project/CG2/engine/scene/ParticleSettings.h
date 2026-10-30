#pragma once

// 自作
#include "EngineStructs.h"
#include "../dx12/BlendMode.h"

// C++
#include <string>

// 保存・履歴用の設定。実行中の粒子やGPUリソースは含めない。
struct ParticleSettings {
	//=============================================================================================//
	// 発生条件

	Vector3 emitterPosition{}; // 個別EmitterはTransformの位置を使う。
	Vector3 spread = {1,1,1}; // 発生位置をランダムに広げる範囲。
	uint32_t spawnCount = 3; // 一回の発生数。最大100。
	float interval = 0.1f; // 発生間隔（秒）。

	//=============================================================================================//
	// 移動・寿命

	Vector3 velocityMinimum = {-1,-1,-1};
	Vector3 velocityMaximum = {1,1,1};
	Vector3 acceleration = {0,1,0};
	bool accelerationEnabled = false;
	float lifetimeMinimum = 1; // 最短寿命（秒）。
	float lifetimeMaximum = 3; // 最長寿命（秒）。

	//=============================================================================================//
	// 表示・合成

	Vector3 particleScale = {1,1,1};
	Vector4 color = {1,0.65f,0.2f,1};
	bool randomColor = false; // 設定色を基準に粒子ごとの色を変える。
	bool billboard = true; // 各粒子をカメラの方へ向ける。
	BlendMode blendMode = kBlendModeAdd;
	std::string texture = "resources/circle.png";
};

// 操作中だけの状態。プリセットとシーンファイルの保存対象には含めない。
struct ParticlePreviewState {
	//=============================================================================================//
	// 対象・再生状態

	uint64_t targetObject = 0; // 0は共通プリセット、それ以外はシーン内EmitterのID。
	bool enabled = false;
	bool paused = false;
	bool step = false; // 次の一回だけ時間を進める。

	//=============================================================================================//
	// 表示・発生モード

	bool isolate = false; // モデルを隠してParticleだけを確認する。
	bool automaticEmission = true; // falseならBurst操作だけで発生する。
};

//=============================================================================================//
// 設定の検証・比較

// 実行前・保存前・読込後に同じ規則で数値とアセット参照を検証する。
bool ValidateParticleSettings(const ParticleSettings& settings);
// 実行中の粒子は比較せず、保存される設定だけを比較する。
bool EqualParticleSettings(const ParticleSettings& firstSettings, const ParticleSettings& secondSettings);
