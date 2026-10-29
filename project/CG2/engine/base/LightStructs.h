#pragma once

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
