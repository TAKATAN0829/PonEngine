#pragma once

// C++
#include <vector>

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
// 頂点データ

struct VertexData {
	Vector4 position;
	Vector2 texcoord;
	Vector3 normal;
};

//=================================================================================================//
// マテリアル

struct MaterialData {
	Vector4 color;
	int32_t enableLighting;
	float padding[3];
	Matrix4x4 uvTransform;
	float shininess;
};

//=================================================================================================//
// TransformationMatrix

struct TransformationMatrix {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Matrix4x4 WorldInverseTranspose;
};

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

//=================================================================================================//
// モデルデータ

struct ModelData {
	std::vector<VertexData> vertices;
};

//=================================================================================================//
// Particle用インスタンシングデータ

struct ParticleForGPU {
	Matrix4x4 WVP;
	Matrix4x4 World;
	Vector4 color;
};

//=================================================================================================//
// カメラ（GPU転送用）

struct CameraForGPU {
	Vector3 worldPosition;
};