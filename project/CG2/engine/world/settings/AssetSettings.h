#pragma once

#include "EngineStructs.h"
#include <cstdint>
#include <string>

// シーン・Prefab・履歴で共有する、GPUリソースを含まないアセット設定。
struct MaterialSettings {
	MaterialSettings();

	uint32_t slot; // モデル内のマテリアル番号。

	std::string texture; // resourcesからの相対パス。空文字ならモデル既定の画像。

	Vector4 color;

	float shininess;

	bool isLightingEnabled;
};

struct AnimationSettings {
	AnimationSettings();

	int clip; // モデルに含まれるクリップの番号。

	bool shouldAutoplay; // ゲーム再生開始時に再生する。
	bool shouldLoop;

	float speed;
	float time; // 開始位置と、Inspectorで指定したシーク位置（秒）。
};

// 設定の比較はUIと履歴で共通にし、見た目の変更をUndoから漏らさない。
bool AreMaterialSettingsEqual(const MaterialSettings& first, const MaterialSettings& second);

bool AreAnimationSettingsEqual(const AnimationSettings& first, const AnimationSettings& second);
