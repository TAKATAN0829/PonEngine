#pragma once

// C++
#include <string>

// 自作
#include "MathUtility.h"

//=================================================================================================//
// Transform情報

struct TransformData {
	Vector3 scale;
	Vector3 rotate;
	Vector3 translate;
};

//=================================================================================================//
// TransformationMatrix

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
};

//=================================================================================================//
// マテリアル（モデル読み込み用）

struct MaterialData {
	std::string textureFilePath;
	uint32_t textureIndex = 0;
};
