#pragma once
#include "MathUtility.h"
#include "EngineStructs.h"
#include "Input.h"

class DebugCamera
{
public:
	//=============================================================================================//
	// 初期化
	void Initialize(int32_t clientWidth, int32_t clientHeight);
	
	//=============================================================================================//
	// 更新 
	void Update(Input* input);
	// 向きを維持し、選択位置から一定距離離れた場所へカメラを移す。
	void Focus(const Vector3& targetPosition);

	//=============================================================================================//
	// Getter

	const Matrix4x4& GetViewProjectionMatrix() const { return viewProjectionMatrix_; }
	const TransformData& GetTransform() const { return transform_; }
	const Matrix4x4& GetCameraMatrix()const { return cameraMatrix_; }

private:
	//=============================================================================================//
	// 行列更新
	void UpdateMatrix();

private:
	Input* input_ = nullptr;

	TransformData transform_{};
	Matrix4x4 cameraMatrix_{};
	Matrix4x4 viewMatrix_{};
	Matrix4x4 projectionMatrix_{};
	Matrix4x4 viewProjectionMatrix_{};

	int32_t clientWidth_ = 0;
	int32_t clientHeight_ = 0;

	float moveSpeed_ = 0.2f;
	float rotateSpeed_ = 0.01f;
};

