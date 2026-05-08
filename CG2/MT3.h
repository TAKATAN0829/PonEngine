#pragma once
#include "Vector3.h"
#include "Matrix4x4.h"

inline constexpr int kColumnWidth = 60;
inline constexpr int kRowHeight = 20;

class MT3 {
public:

	// Vector3--------------------------------------------------------

	// 加算
	static Vector3 Add(const Vector3& v1, const Vector3& v2);

	// 減算
	static Vector3 Subtract(const Vector3& v1, const Vector3& v2);

	// スカラー倍
	static Vector3 Multiply(float scalar, const Vector3& v);

	// 内積
	static float Dot(const Vector3& v1, const Vector3& v2);

	// 長さ
	static float Length(const Vector3& v);

	// 正規化
	static Vector3 Normalize(const Vector3& v);

	// 座標変換
	static Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix);

	// Matrix4x4------------------------------------------------------

	// 加算
	static Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2);

	// 減算
	static Matrix4x4 Subtruct(const Matrix4x4& m1, const Matrix4x4& m2);

	// 行列の積
	static Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2);

	// 逆行列
	static Matrix4x4 Inverse(const Matrix4x4& m);

	// 転置行列
	static Matrix4x4 Transpose(const Matrix4x4& m);

	// 単位行列の作成
	static Matrix4x4 MakeIdentity4x4();

	// 平行移動行列
	static Matrix4x4 MakeTranslateMatrix(const Vector3& translate);

	// 拡大縮小行列
	static Matrix4x4 MakeScaleMatrix(const Vector3& scale);

	// X軸回転行列
	static Matrix4x4 MakeRotateXMatrix(float radian);

	// Y軸回転行列
	static Matrix4x4 MakeRotateYMatrix(float radian);

	// Z軸回転行列
	static Matrix4x4 MakeRotateZMatrix(float radian);

	// アフィン変換
	static Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate);

	// 透視射影行列
	static Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip);

	// 正射影行列
	static Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip);

	// ビューポート行列
	static Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth);
};