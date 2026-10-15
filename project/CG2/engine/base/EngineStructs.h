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