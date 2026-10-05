#pragma once
#include "MathUtility.h"
#include "EngineStructs.h"
#include "Input.h"

class DebugCamera
{
public:
	void Initialize(int32_t clientWidth, int32_t clientHeight);
	void Update(Input* input);

	const Matrix4x4& GetViewProjectionMatrix() const;
	const TransformData& GetTransform() const;
private:
	void UpdateMatrix();

private:
	Input* input_ = nullptr;

	TransformData transform_{};

	Matrix4x4 viewMatrix_{};
	Matrix4x4 projectionMatrix_{};
	Matrix4x4 viewProjectionMatrix_{};

	int32_t clientWidth_ = 0;
	int32_t clientHeight_ = 0;

	float moveSpeed_ = 0.2f;
	float rotateSpeed_ = 0.01f;
};

