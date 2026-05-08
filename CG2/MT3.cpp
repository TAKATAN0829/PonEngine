#include "MT3.h"
#include <cmath>

//=================================================================================================//
// Vector3

// 加算
Vector3 Add(const Vector3& v1, const Vector3& v2) {
	Vector3 result{};

	result.x = v1.x + v2.x;
	result.y = v1.y + v2.y;
	result.z = v1.z + v2.z;

	return result;
}

// 減算
Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
	Vector3 result{};

	result.x = v1.x - v2.x;
	result.y = v1.y - v2.y;
	result.z = v1.z - v2.z;

	return result;
}

// スカラー倍
Vector3 Multiply(float scalar, const Vector3& v) {
	Vector3 result{};

	result.x = scalar * v.x;
	result.y = scalar * v.y;
	result.z = scalar * v.z;

	return result;
}

// 内積
float Dot(const Vector3& v1, const Vector3& v2) {
	float result = 0.0f;

	result = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;

	return result;
}

// 長さ
float Length(const Vector3& v) {
	float result = 0.0f;

	result = std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);

	return result;
}

// 正規化
Vector3 Normalize(const Vector3& v) {
	Vector3 result{};
	float length = Length(v);

	if (length == 0.0f) {
		return result;
	}

	result.x = v.x / length;
	result.y = v.y / length;
	result.z = v.z / length;

	return result;
}

// 座標変換
Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix) {
	Vector3 result{};

	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];
	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];
	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];

	float w = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];

	if (w != 0.0f) {
		result.x /= w;
		result.y /= w;
		result.z /= w;
	}

	return result;
}

//=================================================================================================//


//=================================================================================================//
// Matrix4x4

// 加算
Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 resultMatrix{};

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			resultMatrix.m[row][col] = m1.m[row][col] + m2.m[row][col];
		}
	}

	return resultMatrix;
}

// 減算
Matrix4x4 Subtruct(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 resultMatrix{};

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			resultMatrix.m[row][col] = m1.m[row][col] - m2.m[row][col];
		}
	}

	return resultMatrix;
}

// 行列の積
Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
	Matrix4x4 resultMatrix{};

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			resultMatrix.m[row][col] = 0.f;

			for (int k = 0; k < 4; ++k) {
				resultMatrix.m[row][col] += m1.m[row][k] * m2.m[k][col];
			}
		}
	}

	return resultMatrix;
}

// 逆行列
Matrix4x4 Inverse(const Matrix4x4& m) {
	float a =
		m.m[0][0] * m.m[1][1] * m.m[2][2] * m.m[3][3]
		- m.m[0][0] * m.m[1][1] * m.m[2][3] * m.m[3][2]
		- m.m[0][0] * m.m[1][2] * m.m[2][1] * m.m[3][3]
		+ m.m[0][0] * m.m[1][2] * m.m[2][3] * m.m[3][1]
		+ m.m[0][0] * m.m[1][3] * m.m[2][1] * m.m[3][2]
		- m.m[0][0] * m.m[1][3] * m.m[2][2] * m.m[3][1]
		- m.m[0][1] * m.m[1][0] * m.m[2][2] * m.m[3][3]
		+ m.m[0][1] * m.m[1][0] * m.m[2][3] * m.m[3][2]
		+ m.m[0][1] * m.m[1][2] * m.m[2][0] * m.m[3][3]
		- m.m[0][1] * m.m[1][2] * m.m[2][3] * m.m[3][0]
		- m.m[0][1] * m.m[1][3] * m.m[2][0] * m.m[3][2]
		+ m.m[0][1] * m.m[1][3] * m.m[2][2] * m.m[3][0]
		+ m.m[0][2] * m.m[1][0] * m.m[2][1] * m.m[3][3]
		- m.m[0][2] * m.m[1][0] * m.m[2][3] * m.m[3][1]
		- m.m[0][2] * m.m[1][1] * m.m[2][0] * m.m[3][3]
		+ m.m[0][2] * m.m[1][1] * m.m[2][3] * m.m[3][0]
		+ m.m[0][2] * m.m[1][3] * m.m[2][0] * m.m[3][1]
		- m.m[0][2] * m.m[1][3] * m.m[2][1] * m.m[3][0]
		- m.m[0][3] * m.m[1][0] * m.m[2][1] * m.m[3][2]
		+ m.m[0][3] * m.m[1][0] * m.m[2][2] * m.m[3][1]
		+ m.m[0][3] * m.m[1][1] * m.m[2][0] * m.m[3][2]
		- m.m[0][3] * m.m[1][1] * m.m[2][2] * m.m[3][0]
		- m.m[0][3] * m.m[1][2] * m.m[2][0] * m.m[3][1]
		+ m.m[0][3] * m.m[1][2] * m.m[2][1] * m.m[3][0];

	Matrix4x4 resultMatrix{};

	resultMatrix.m[0][0] = (
		+m.m[1][1] * m.m[2][2] * m.m[3][3]
		- m.m[1][1] * m.m[2][3] * m.m[3][2]
		- m.m[1][2] * m.m[2][1] * m.m[3][3]
		+ m.m[1][2] * m.m[2][3] * m.m[3][1]
		+ m.m[1][3] * m.m[2][1] * m.m[3][2]
		- m.m[1][3] * m.m[2][2] * m.m[3][1]) / a;

	resultMatrix.m[0][1] = (
		-m.m[0][1] * m.m[2][2] * m.m[3][3]
		+ m.m[0][1] * m.m[2][3] * m.m[3][2]
		+ m.m[0][2] * m.m[2][1] * m.m[3][3]
		- m.m[0][2] * m.m[2][3] * m.m[3][1]
		- m.m[0][3] * m.m[2][1] * m.m[3][2]
		+ m.m[0][3] * m.m[2][2] * m.m[3][1]) / a;

	resultMatrix.m[0][2] = (
		+m.m[0][1] * m.m[1][2] * m.m[3][3]
		- m.m[0][1] * m.m[1][3] * m.m[3][2]
		- m.m[0][2] * m.m[1][1] * m.m[3][3]
		+ m.m[0][2] * m.m[1][3] * m.m[3][1]
		+ m.m[0][3] * m.m[1][1] * m.m[3][2]
		- m.m[0][3] * m.m[1][2] * m.m[3][1]) / a;

	resultMatrix.m[0][3] = (
		-m.m[0][1] * m.m[1][2] * m.m[2][3]
		+ m.m[0][1] * m.m[1][3] * m.m[2][2]
		+ m.m[0][2] * m.m[1][1] * m.m[2][3]
		- m.m[0][2] * m.m[1][3] * m.m[2][1]
		- m.m[0][3] * m.m[1][1] * m.m[2][2]
		+ m.m[0][3] * m.m[1][2] * m.m[2][1]) / a;

	resultMatrix.m[1][0] = (
		-m.m[1][0] * m.m[2][2] * m.m[3][3]
		+ m.m[1][0] * m.m[2][3] * m.m[3][2]
		+ m.m[1][2] * m.m[2][0] * m.m[3][3]
		- m.m[1][2] * m.m[2][3] * m.m[3][0]
		- m.m[1][3] * m.m[2][0] * m.m[3][2]
		+ m.m[1][3] * m.m[2][2] * m.m[3][0]) / a;

	resultMatrix.m[1][1] = (
		+m.m[0][0] * m.m[2][2] * m.m[3][3]
		- m.m[0][0] * m.m[2][3] * m.m[3][2]
		- m.m[0][2] * m.m[2][0] * m.m[3][3]
		+ m.m[0][2] * m.m[2][3] * m.m[3][0]
		+ m.m[0][3] * m.m[2][0] * m.m[3][2]
		- m.m[0][3] * m.m[2][2] * m.m[3][0]) / a;

	resultMatrix.m[1][2] = (
		-m.m[0][0] * m.m[1][2] * m.m[3][3]
		+ m.m[0][0] * m.m[1][3] * m.m[3][2]
		+ m.m[0][2] * m.m[1][0] * m.m[3][3]
		- m.m[0][2] * m.m[1][3] * m.m[3][0]
		- m.m[0][3] * m.m[1][0] * m.m[3][2]
		+ m.m[0][3] * m.m[1][2] * m.m[3][0]) / a;

	resultMatrix.m[1][3] = (
		+m.m[0][0] * m.m[1][2] * m.m[2][3]
		- m.m[0][0] * m.m[1][3] * m.m[2][2]
		- m.m[0][2] * m.m[1][0] * m.m[2][3]
		+ m.m[0][2] * m.m[1][3] * m.m[2][0]
		+ m.m[0][3] * m.m[1][0] * m.m[2][2]
		- m.m[0][3] * m.m[1][2] * m.m[2][0]) / a;

	resultMatrix.m[2][0] = (
		+m.m[1][0] * m.m[2][1] * m.m[3][3]
		- m.m[1][0] * m.m[2][3] * m.m[3][1]
		- m.m[1][1] * m.m[2][0] * m.m[3][3]
		+ m.m[1][1] * m.m[2][3] * m.m[3][0]
		+ m.m[1][3] * m.m[2][0] * m.m[3][1]
		- m.m[1][3] * m.m[2][1] * m.m[3][0]) / a;

	resultMatrix.m[2][1] = (
		-m.m[0][0] * m.m[2][1] * m.m[3][3]
		+ m.m[0][0] * m.m[2][3] * m.m[3][1]
		+ m.m[0][1] * m.m[2][0] * m.m[3][3]
		- m.m[0][1] * m.m[2][3] * m.m[3][0]
		- m.m[0][3] * m.m[2][0] * m.m[3][1]
		+ m.m[0][3] * m.m[2][1] * m.m[3][0]) / a;

	resultMatrix.m[2][2] = (
		+m.m[0][0] * m.m[1][1] * m.m[3][3]
		- m.m[0][0] * m.m[1][3] * m.m[3][1]
		- m.m[0][1] * m.m[1][0] * m.m[3][3]
		+ m.m[0][1] * m.m[1][3] * m.m[3][0]
		+ m.m[0][3] * m.m[1][0] * m.m[3][1]
		- m.m[0][3] * m.m[1][1] * m.m[3][0]) / a;

	resultMatrix.m[2][3] = (
		-m.m[0][0] * m.m[1][1] * m.m[2][3]
		+ m.m[0][0] * m.m[1][3] * m.m[2][1]
		+ m.m[0][1] * m.m[1][0] * m.m[2][3]
		- m.m[0][1] * m.m[1][3] * m.m[2][0]
		- m.m[0][3] * m.m[1][0] * m.m[2][1]
		+ m.m[0][3] * m.m[1][1] * m.m[2][0]) / a;

	resultMatrix.m[3][0] = (
		-m.m[1][0] * m.m[2][1] * m.m[3][2]
		+ m.m[1][0] * m.m[2][2] * m.m[3][1]
		+ m.m[1][1] * m.m[2][0] * m.m[3][2]
		- m.m[1][1] * m.m[2][2] * m.m[3][0]
		- m.m[1][2] * m.m[2][0] * m.m[3][1]
		+ m.m[1][2] * m.m[2][1] * m.m[3][0]) / a;

	resultMatrix.m[3][1] = (
		+m.m[0][0] * m.m[2][1] * m.m[3][2]
		- m.m[0][0] * m.m[2][2] * m.m[3][1]
		- m.m[0][1] * m.m[2][0] * m.m[3][2]
		+ m.m[0][1] * m.m[2][2] * m.m[3][0]
		+ m.m[0][2] * m.m[2][0] * m.m[3][1]
		- m.m[0][2] * m.m[2][1] * m.m[3][0]) / a;

	resultMatrix.m[3][2] = (
		-m.m[0][0] * m.m[1][1] * m.m[3][2]
		+ m.m[0][0] * m.m[1][2] * m.m[3][1]
		+ m.m[0][1] * m.m[1][0] * m.m[3][2]
		- m.m[0][1] * m.m[1][2] * m.m[3][0]
		- m.m[0][2] * m.m[1][0] * m.m[3][1]
		+ m.m[0][2] * m.m[1][1] * m.m[3][0]) / a;

	resultMatrix.m[3][3] = (
		+m.m[0][0] * m.m[1][1] * m.m[2][2]
		- m.m[0][0] * m.m[1][2] * m.m[2][1]
		- m.m[0][1] * m.m[1][0] * m.m[2][2]
		+ m.m[0][1] * m.m[1][2] * m.m[2][0]
		+ m.m[0][2] * m.m[1][0] * m.m[2][1]
		- m.m[0][2] * m.m[1][1] * m.m[2][0]) / a;

	return resultMatrix;
}

// 転置行列
Matrix4x4 Transpose(const Matrix4x4& m) {
	Matrix4x4 resultMatrix{};

	for (int row = 0; row < 4; ++row) {
		for (int col = 0; col < 4; ++col) {
			resultMatrix.m[row][col] = m.m[col][row];
		}
	}

	return resultMatrix;
}

// 単位行列の作成
Matrix4x4 MakeIdentity4x4() {
	Matrix4x4 resultMatrix{};

	for (int i = 0; i < 4; ++i) {
		resultMatrix.m[i][i] = 1.f;
	}

	return resultMatrix;
}

// 平行移動行列
Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
	Matrix4x4 resultMatrix = MakeIdentity4x4();

	resultMatrix.m[3][0] = translate.x;
	resultMatrix.m[3][1] = translate.y;
	resultMatrix.m[3][2] = translate.z;

	return resultMatrix;
}

// 拡大縮小行列
Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
	Matrix4x4 resultMatrix{};

	resultMatrix.m[0][0] = scale.x;
	resultMatrix.m[1][1] = scale.y;
	resultMatrix.m[2][2] = scale.z;
	resultMatrix.m[3][3] = 1.0f;

	return resultMatrix;
}

// X軸回転行列
Matrix4x4 MakeRotateXMatrix(float radian) {
	Matrix4x4 resultMatrix = MakeIdentity4x4();

	resultMatrix.m[1][1] = std::cos(radian);
	resultMatrix.m[1][2] = std::sin(radian);

	resultMatrix.m[2][1] = -std::sin(radian);
	resultMatrix.m[2][2] = std::cos(radian);

	return resultMatrix;
}

// Y軸回転行列
Matrix4x4 MakeRotateYMatrix(float radian) {
	Matrix4x4 resultMatrix = MakeIdentity4x4();

	resultMatrix.m[0][0] = std::cos(radian);
	resultMatrix.m[0][2] = -std::sin(radian);

	resultMatrix.m[2][0] = std::sin(radian);
	resultMatrix.m[2][2] = std::cos(radian);

	return resultMatrix;
}

// Z軸回転行列
Matrix4x4 MakeRotateZMatrix(float radian) {
	Matrix4x4 resultMatrix = MakeIdentity4x4();

	resultMatrix.m[0][0] = std::cos(radian);
	resultMatrix.m[0][1] = std::sin(radian);

	resultMatrix.m[1][0] = -std::sin(radian);
	resultMatrix.m[1][1] = std::cos(radian);

	return resultMatrix;
}

// アフィン変換
Matrix4x4  MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {

	// 拡大縮小行列
	Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

	// 回転行列
	Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
	Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
	Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);

	// 平行移動行列
	Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

	// 回転行列を合成
	Matrix4x4 rotateMatrix = Multiply(Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);

	// SRT
	Matrix4x4 srMatrix = Multiply(scaleMatrix, rotateMatrix);
	Matrix4x4 srtMatrix = Multiply(srMatrix, translateMatrix);

	return srtMatrix;
}

// 透視射影行列
Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
	Matrix4x4 resultMatrix{};
	float f = 1.f / std::tanf(fovY / 2.f);
	resultMatrix.m[0][0] = f / aspectRatio;
	resultMatrix.m[1][1] = f;
	resultMatrix.m[2][2] = farClip / (farClip - nearClip);
	resultMatrix.m[2][3] = 1.f;
	resultMatrix.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);

	return resultMatrix;
}

// 正射影行列
Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
	Matrix4x4 resultMatrix{};
	resultMatrix.m[0][0] = 2.f / (right - left);
	resultMatrix.m[1][1] = 2.f / (top - bottom);
	resultMatrix.m[2][2] = 1.f / (farClip - nearClip);
	resultMatrix.m[3][0] = (left + right) / (left - right);
	resultMatrix.m[3][1] = (top + bottom) / (bottom - top);
	resultMatrix.m[3][2] = nearClip / (nearClip - farClip);
	resultMatrix.m[3][3] = 1.f;

	return resultMatrix;
}

// ビューポート行列
Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
	Matrix4x4 resultMatrix{};
	resultMatrix.m[0][0] = width / 2.f;
	resultMatrix.m[1][1] = -height / 2.f;
	resultMatrix.m[2][2] = maxDepth - minDepth;
	resultMatrix.m[3][0] = left + (width / 2.f);
	resultMatrix.m[3][1] = top + (height / 2.f);
	resultMatrix.m[3][2] = minDepth;
	resultMatrix.m[3][3] = 1.f;

	return resultMatrix;
}

//=================================================================================================//