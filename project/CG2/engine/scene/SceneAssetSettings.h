#pragma once
#include "EngineStructs.h"
#include <cstdint>
#include <string>
#include <vector>

// シーン・Prefab・履歴で共有する、GPUリソースを含まないアセット設定。
struct SceneMaterialSettings {
	uint32_t slot = 0; // モデル内のマテリアル番号。
	std::string texture; // resourcesからの相対パス。空文字ならモデル既定の画像。
	Vector4 color = {1, 1, 1, 1};
	float shininess = 40;
	bool lighting = true;
};

struct SceneAnimationSettings {
	int clip = 0; // モデルに含まれるクリップの番号。
	bool autoplay = true; // ゲーム再生開始時に再生する。
	bool loop = true;
	float speed = 1;
	float time = 0; // 開始位置と、Inspectorで指定したシーク位置（秒）。
};

struct ModelMaterialInfo {
	std::string name;
	std::string texture;
	Vector4 color = {1, 1, 1, 1};
};

struct ModelAnimationInfo {
	std::string name;
	float duration = 0;
};

// 設定の比較はUIと履歴で共通にし、見た目の変更をUndoから漏らさない。
inline bool EqualMaterialSettings(const SceneMaterialSettings& first, const SceneMaterialSettings& second) {
	return first.slot == second.slot && first.texture == second.texture && first.color.x == second.color.x &&
		first.color.y == second.color.y && first.color.z == second.color.z && first.color.w == second.color.w &&
		first.shininess == second.shininess && first.lighting == second.lighting;
}

inline bool EqualAnimationSettings(const SceneAnimationSettings& first, const SceneAnimationSettings& second) {
	return first.clip == second.clip && first.autoplay == second.autoplay && first.loop == second.loop &&
		first.speed == second.speed && first.time == second.time;
}
