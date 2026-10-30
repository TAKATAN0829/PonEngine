#pragma once
#include <cstdint>

// 自作
#include "MathUtility.h"

//=================================================================================================//
// 平行光源

struct DirectionalLight {
	Vector4 color;
	Vector3 direction;
	float intensity;
};

//=================================================================================================//
// 点光源

struct PointLight {
	Vector4 color;
	Vector3 position;
	float intensity;
	float radius;
	float decay;
	float padding[2];
};

// CPUとHLSLで16バイト境界と配列の要素サイズを揃える。
struct PointLightBuffer {
    uint32_t count = 0;
    float padding[3]{};
    PointLight lights[9]{}; // World Settingsの光源1個と、配置する光源8個。
};
static_assert(sizeof(PointLight) == 48);
static_assert(sizeof(PointLightBuffer) == 448);

//=================================================================================================//
// スポットライト

struct SpotLight {
	Vector4 color;
	Vector3 position;
	float intensity;
	Vector3 direction;
	float distance;
	float decay;
	float cosAngle;
	float cosFalloffStart;
	float padding[2];
};
