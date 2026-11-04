#pragma once

#include <cstdint>
#include "LightLimits.h"

// 自作
#include "MathUtility.h"

//=================================================================================================//
// 平行光源

struct DirectionalLight {
	Vector4 color;

	Vector3 direction;

	float intensity;
};

struct DirectionalLightBuffer {
	uint32_t count = 0;

	float padding[3]{};

	DirectionalLight lights[kMaxDirectionalLights]{};
};

static_assert(sizeof(DirectionalLight) == 32);

static_assert(sizeof(DirectionalLightBuffer) == 144);

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

    PointLight lights[kMaxPointLights]{};
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
	float padding[1]; // HLSLの配列要素も64バイトに揃える。
};

struct SpotLightBuffer {
	uint32_t count = 0;

	float padding[3]{};

	SpotLight lights[kMaxSpotLights]{};
};

static_assert(sizeof(SpotLight) == 64);

static_assert(sizeof(SpotLightBuffer) == 528);

// 毎フレーム一度だけ収集する。固定長なのでライト一覧の動的確保は不要。
struct WorldLightCollection {
	DirectionalLightBuffer directional;

	PointLightBuffer points;

	SpotLightBuffer spots;
};
