#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"
#include "AABB.h"

// ベクトル・行列・衝突判定の共通計算をまとめる。
namespace MathUtility {

	//=============================================================================================//
	// Vector3

	Vector3 Add(const Vector3& firstVector, const Vector3& secondVector);

	Vector3 Subtract(const Vector3& firstVector, const Vector3& secondVector);

	Vector3 Multiply(float scalar, const Vector3& vector);

	float Dot(const Vector3& firstVector, const Vector3& secondVector);

	float Length(const Vector3& vector);

	Vector3 Normalize(const Vector3& vector);

	Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

	Vector3 TransformNormal(const Vector3& vector, const Matrix4x4& matrix);

	//=============================================================================================//
	// Matrix4x4

	Matrix4x4 Add(const Matrix4x4& firstMatrix, const Matrix4x4& secondMatrix);

	Matrix4x4 Subtract(const Matrix4x4& firstMatrix, const Matrix4x4& secondMatrix);

	Matrix4x4 Multiply(const Matrix4x4& firstMatrix, const Matrix4x4& secondMatrix);

	Matrix4x4 Inverse(const Matrix4x4& matrix);

	Matrix4x4 Transpose(const Matrix4x4& matrix);

	Matrix4x4 MakeIdentity4x4();

	Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	Matrix4x4 MakeRotateXMatrix(float radian);

	Matrix4x4 MakeRotateYMatrix(float radian);

	Matrix4x4 MakeRotateZMatrix(float radian);

	Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	Matrix4x4 MakePerspectiveFovMatrix(
		float fovY,
		float aspectRatio,
		float nearClip,
		float farClip);

	Matrix4x4 MakeOrthographicMatrix(
		float left,
		float top,
		float right,
		float bottom,
		float nearClip,
		float farClip);

	Matrix4x4 MakeViewportMatrix(
		float left,
		float top,
		float width,
		float height,
		float minDepth,
		float maxDepth);

	bool IsCollision(const AABB& aabb, const Vector3& point);
}
