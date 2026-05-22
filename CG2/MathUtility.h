#pragma once

#include "Vector2.h"
#include "Vector3.h"
#include "Vector4.h"
#include "Matrix4x4.h"

#include <cmath>

class MathUtility {
public:

	// Vector3

	static inline Vector3 Add(const Vector3& v1, const Vector3& v2) {
		Vector3 result{};

		result.x = v1.x + v2.x;
		result.y = v1.y + v2.y;
		result.z = v1.z + v2.z;

		return result;
	}

	static inline Vector3 Subtract(const Vector3& v1, const Vector3& v2) {
		Vector3 result{};

		result.x = v1.x - v2.x;
		result.y = v1.y - v2.y;
		result.z = v1.z - v2.z;

		return result;
	}

	static inline Vector3 Multiply(float scalar, const Vector3& v) {
		Vector3 result{};

		result.x = scalar * v.x;
		result.y = scalar * v.y;
		result.z = scalar * v.z;

		return result;
	}

	static inline float Dot(const Vector3& v1, const Vector3& v2) {
		return v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
	}

	static inline float Length(const Vector3& v) {
		return std::sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
	}

	static inline Vector3 Normalize(const Vector3& v) {
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

	static inline Vector3 Transform(const Vector3& vector, const Matrix4x4& matrix) {
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

	// Matrix4x4

	static inline Matrix4x4 Add(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result{};

		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				result.m[row][col] = m1.m[row][col] + m2.m[row][col];
			}
		}

		return result;
	}

	static inline Matrix4x4 Subtract(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result{};

		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				result.m[row][col] = m1.m[row][col] - m2.m[row][col];
			}
		}

		return result;
	}

	static inline Matrix4x4 Multiply(const Matrix4x4& m1, const Matrix4x4& m2) {
		Matrix4x4 result{};

		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				result.m[row][col] = 0.0f;

				for (int k = 0; k < 4; ++k) {
					result.m[row][col] += m1.m[row][k] * m2.m[k][col];
				}
			}
		}

		return result;
	}

	static inline Matrix4x4 Inverse(const Matrix4x4& m) {
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

		Matrix4x4 result{};

		result.m[0][0] = (+m.m[1][1] * m.m[2][2] * m.m[3][3] - m.m[1][1] * m.m[2][3] * m.m[3][2] - m.m[1][2] * m.m[2][1] * m.m[3][3] + m.m[1][2] * m.m[2][3] * m.m[3][1] + m.m[1][3] * m.m[2][1] * m.m[3][2] - m.m[1][3] * m.m[2][2] * m.m[3][1]) / a;
		result.m[0][1] = (-m.m[0][1] * m.m[2][2] * m.m[3][3] + m.m[0][1] * m.m[2][3] * m.m[3][2] + m.m[0][2] * m.m[2][1] * m.m[3][3] - m.m[0][2] * m.m[2][3] * m.m[3][1] - m.m[0][3] * m.m[2][1] * m.m[3][2] + m.m[0][3] * m.m[2][2] * m.m[3][1]) / a;
		result.m[0][2] = (+m.m[0][1] * m.m[1][2] * m.m[3][3] - m.m[0][1] * m.m[1][3] * m.m[3][2] - m.m[0][2] * m.m[1][1] * m.m[3][3] + m.m[0][2] * m.m[1][3] * m.m[3][1] + m.m[0][3] * m.m[1][1] * m.m[3][2] - m.m[0][3] * m.m[1][2] * m.m[3][1]) / a;
		result.m[0][3] = (-m.m[0][1] * m.m[1][2] * m.m[2][3] + m.m[0][1] * m.m[1][3] * m.m[2][2] + m.m[0][2] * m.m[1][1] * m.m[2][3] - m.m[0][2] * m.m[1][3] * m.m[2][1] - m.m[0][3] * m.m[1][1] * m.m[2][2] + m.m[0][3] * m.m[1][2] * m.m[2][1]) / a;

		result.m[1][0] = (-m.m[1][0] * m.m[2][2] * m.m[3][3] + m.m[1][0] * m.m[2][3] * m.m[3][2] + m.m[1][2] * m.m[2][0] * m.m[3][3] - m.m[1][2] * m.m[2][3] * m.m[3][0] - m.m[1][3] * m.m[2][0] * m.m[3][2] + m.m[1][3] * m.m[2][2] * m.m[3][0]) / a;
		result.m[1][1] = (+m.m[0][0] * m.m[2][2] * m.m[3][3] - m.m[0][0] * m.m[2][3] * m.m[3][2] - m.m[0][2] * m.m[2][0] * m.m[3][3] + m.m[0][2] * m.m[2][3] * m.m[3][0] + m.m[0][3] * m.m[2][0] * m.m[3][2] - m.m[0][3] * m.m[2][2] * m.m[3][0]) / a;
		result.m[1][2] = (-m.m[0][0] * m.m[1][2] * m.m[3][3] + m.m[0][0] * m.m[1][3] * m.m[3][2] + m.m[0][2] * m.m[1][0] * m.m[3][3] - m.m[0][2] * m.m[1][3] * m.m[3][0] - m.m[0][3] * m.m[1][0] * m.m[3][2] + m.m[0][3] * m.m[1][2] * m.m[3][0]) / a;
		result.m[1][3] = (+m.m[0][0] * m.m[1][2] * m.m[2][3] - m.m[0][0] * m.m[1][3] * m.m[2][2] - m.m[0][2] * m.m[1][0] * m.m[2][3] + m.m[0][2] * m.m[1][3] * m.m[2][0] + m.m[0][3] * m.m[1][0] * m.m[2][2] - m.m[0][3] * m.m[1][2] * m.m[2][0]) / a;

		result.m[2][0] = (+m.m[1][0] * m.m[2][1] * m.m[3][3] - m.m[1][0] * m.m[2][3] * m.m[3][1] - m.m[1][1] * m.m[2][0] * m.m[3][3] + m.m[1][1] * m.m[2][3] * m.m[3][0] + m.m[1][3] * m.m[2][0] * m.m[3][1] - m.m[1][3] * m.m[2][1] * m.m[3][0]) / a;
		result.m[2][1] = (-m.m[0][0] * m.m[2][1] * m.m[3][3] + m.m[0][0] * m.m[2][3] * m.m[3][1] + m.m[0][1] * m.m[2][0] * m.m[3][3] - m.m[0][1] * m.m[2][3] * m.m[3][0] - m.m[0][3] * m.m[2][0] * m.m[3][1] + m.m[0][3] * m.m[2][1] * m.m[3][0]) / a;
		result.m[2][2] = (+m.m[0][0] * m.m[1][1] * m.m[3][3] - m.m[0][0] * m.m[1][3] * m.m[3][1] - m.m[0][1] * m.m[1][0] * m.m[3][3] + m.m[0][1] * m.m[1][3] * m.m[3][0] + m.m[0][3] * m.m[1][0] * m.m[3][1] - m.m[0][3] * m.m[1][1] * m.m[3][0]) / a;
		result.m[2][3] = (-m.m[0][0] * m.m[1][1] * m.m[2][3] + m.m[0][0] * m.m[1][3] * m.m[2][1] + m.m[0][1] * m.m[1][0] * m.m[2][3] - m.m[0][1] * m.m[1][3] * m.m[2][0] - m.m[0][3] * m.m[1][0] * m.m[2][1] + m.m[0][3] * m.m[1][1] * m.m[2][0]) / a;

		result.m[3][0] = (-m.m[1][0] * m.m[2][1] * m.m[3][2] + m.m[1][0] * m.m[2][2] * m.m[3][1] + m.m[1][1] * m.m[2][0] * m.m[3][2] - m.m[1][1] * m.m[2][2] * m.m[3][0] - m.m[1][2] * m.m[2][0] * m.m[3][1] + m.m[1][2] * m.m[2][1] * m.m[3][0]) / a;
		result.m[3][1] = (+m.m[0][0] * m.m[2][1] * m.m[3][2] - m.m[0][0] * m.m[2][2] * m.m[3][1] - m.m[0][1] * m.m[2][0] * m.m[3][2] + m.m[0][1] * m.m[2][2] * m.m[3][0] + m.m[0][2] * m.m[2][0] * m.m[3][1] - m.m[0][2] * m.m[2][1] * m.m[3][0]) / a;
		result.m[3][2] = (-m.m[0][0] * m.m[1][1] * m.m[3][2] + m.m[0][0] * m.m[1][2] * m.m[3][1] + m.m[0][1] * m.m[1][0] * m.m[3][2] - m.m[0][1] * m.m[1][2] * m.m[3][0] - m.m[0][2] * m.m[1][0] * m.m[3][1] + m.m[0][2] * m.m[1][1] * m.m[3][0]) / a;
		result.m[3][3] = (+m.m[0][0] * m.m[1][1] * m.m[2][2] - m.m[0][0] * m.m[1][2] * m.m[2][1] - m.m[0][1] * m.m[1][0] * m.m[2][2] + m.m[0][1] * m.m[1][2] * m.m[2][0] + m.m[0][2] * m.m[1][0] * m.m[2][1] - m.m[0][2] * m.m[1][1] * m.m[2][0]) / a;

		return result;
	}

	static inline Matrix4x4 Transpose(const Matrix4x4& m) {
		Matrix4x4 result{};

		for (int row = 0; row < 4; ++row) {
			for (int col = 0; col < 4; ++col) {
				result.m[row][col] = m.m[col][row];
			}
		}

		return result;
	}

	static inline Matrix4x4 MakeIdentity4x4() {
		Matrix4x4 result{};

		for (int i = 0; i < 4; ++i) {
			result.m[i][i] = 1.0f;
		}

		return result;
	}

	static inline Matrix4x4 MakeTranslateMatrix(const Vector3& translate) {
		Matrix4x4 result = MakeIdentity4x4();

		result.m[3][0] = translate.x;
		result.m[3][1] = translate.y;
		result.m[3][2] = translate.z;

		return result;
	}

	static inline Matrix4x4 MakeScaleMatrix(const Vector3& scale) {
		Matrix4x4 result{};

		result.m[0][0] = scale.x;
		result.m[1][1] = scale.y;
		result.m[2][2] = scale.z;
		result.m[3][3] = 1.0f;

		return result;
	}

	static inline Matrix4x4 MakeRotateXMatrix(float radian) {
		Matrix4x4 result = MakeIdentity4x4();

		result.m[1][1] = std::cos(radian);
		result.m[1][2] = std::sin(radian);
		result.m[2][1] = -std::sin(radian);
		result.m[2][2] = std::cos(radian);

		return result;
	}

	static inline Matrix4x4 MakeRotateYMatrix(float radian) {
		Matrix4x4 result = MakeIdentity4x4();

		result.m[0][0] = std::cos(radian);
		result.m[0][2] = -std::sin(radian);
		result.m[2][0] = std::sin(radian);
		result.m[2][2] = std::cos(radian);

		return result;
	}

	static inline Matrix4x4 MakeRotateZMatrix(float radian) {
		Matrix4x4 result = MakeIdentity4x4();

		result.m[0][0] = std::cos(radian);
		result.m[0][1] = std::sin(radian);
		result.m[1][0] = -std::sin(radian);
		result.m[1][1] = std::cos(radian);

		return result;
	}

	static inline Matrix4x4 MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
		Matrix4x4 scaleMatrix = MakeScaleMatrix(scale);

		Matrix4x4 rotateXMatrix = MakeRotateXMatrix(rotate.x);
		Matrix4x4 rotateYMatrix = MakeRotateYMatrix(rotate.y);
		Matrix4x4 rotateZMatrix = MakeRotateZMatrix(rotate.z);

		Matrix4x4 translateMatrix = MakeTranslateMatrix(translate);

		Matrix4x4 rotateMatrix = Multiply(Multiply(rotateXMatrix, rotateYMatrix), rotateZMatrix);
		Matrix4x4 srMatrix = Multiply(scaleMatrix, rotateMatrix);
		Matrix4x4 srtMatrix = Multiply(srMatrix, translateMatrix);

		return srtMatrix;
	}

	static inline Matrix4x4 MakePerspectiveFovMatrix(float fovY, float aspectRatio, float nearClip, float farClip) {
		Matrix4x4 result{};

		float f = 1.0f / std::tanf(fovY / 2.0f);

		result.m[0][0] = f / aspectRatio;
		result.m[1][1] = f;
		result.m[2][2] = farClip / (farClip - nearClip);
		result.m[2][3] = 1.0f;
		result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);

		return result;
	}

	static inline Matrix4x4 MakeOrthographicMatrix(float left, float top, float right, float bottom, float nearClip, float farClip) {
		Matrix4x4 result{};

		result.m[0][0] = 2.0f / (right - left);
		result.m[1][1] = 2.0f / (top - bottom);
		result.m[2][2] = 1.0f / (farClip - nearClip);
		result.m[3][0] = (left + right) / (left - right);
		result.m[3][1] = (top + bottom) / (bottom - top);
		result.m[3][2] = nearClip / (nearClip - farClip);
		result.m[3][3] = 1.0f;

		return result;
	}

	static inline Matrix4x4 MakeViewportMatrix(float left, float top, float width, float height, float minDepth, float maxDepth) {
		Matrix4x4 result{};

		result.m[0][0] = width / 2.0f;
		result.m[1][1] = -height / 2.0f;
		result.m[2][2] = maxDepth - minDepth;
		result.m[3][0] = left + (width / 2.0f);
		result.m[3][1] = top + (height / 2.0f);
		result.m[3][2] = minDepth;
		result.m[3][3] = 1.0f;

		return result;
	}
};