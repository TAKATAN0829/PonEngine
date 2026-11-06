#pragma once

#include "Vector4.h"
#include <string>

// モデルファイルから読み取ったマテリアルの名前と既定値。
struct ModelMaterialInfo {
	std::string name;
	std::string texture;

	Vector4 color = {1, 1, 1, 1};
};

// モデルに含まれるアニメーションクリップの名前と長さ。
struct ModelAnimationInfo {
	std::string name;

	float duration = 0; // 秒。
};
