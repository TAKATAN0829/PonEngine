#include "MathUtility.h"
#include <cmath>

//=============================================================================================//
// Vector3

Vector3 MathUtility::Add(const Vector3& firstVector, const Vector3& secondVector) {
	// 対応する成分を加算して結果を作る。
	Vector3 result{};

	result.x = firstVector.x + secondVector.x;

	result.y = firstVector.y + secondVector.y;

	result.z = firstVector.z + secondVector.z;

	return result;
}

Vector3 MathUtility::Subtract(const Vector3& firstVector, const Vector3& secondVector) {
	// 対応する成分を減算して結果を作る。
	Vector3 result{};

	result.x = firstVector.x - secondVector.x;

	result.y = firstVector.y - secondVector.y;

	result.z = firstVector.z - secondVector.z;

	return result;
}

Vector3 MathUtility::Multiply(float scalar, const Vector3& vector) {
	// ベクトルの各成分へ、同じ倍率を掛ける。
	Vector3 result{};

	result.x = scalar * vector.x;

	result.y = scalar * vector.y;

	result.z = scalar * vector.z;

	return result;
}

float MathUtility::Dot(const Vector3& firstVector, const Vector3& secondVector) {
	// 対応する成分の積を合計し、内積を求める。
	return firstVector.x * secondVector.x + firstVector.y * secondVector.y + firstVector.z * secondVector.z;
}

float MathUtility::Length(const Vector3& vector) {
	// 各成分の二乗和からベクトルの長さを求める。
	return std::sqrtf(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
}

Vector3 MathUtility::Normalize(const Vector3& vector) {
	// 長さが0でないことを確認し、各成分を長さで割る。
	Vector3 result{};

	float length = Length(vector);

	// 長さ0のベクトルを割り算に使わない。
	if (length == 0.0f) {
		return result;
	}

	result.x = vector.x / length;

	result.y = vector.y / length;

	result.z = vector.z / length;

	return result;
}

Vector3 MathUtility::Transform(const Vector3& vector, const Matrix4x4& matrix) {
	// 行列を適用した後、透視除算して変換後の位置を求める。
	Vector3 result{};

	result.x = vector.x * matrix.m[0][0] + vector.y * matrix.m[1][0] + vector.z * matrix.m[2][0] + matrix.m[3][0];

	result.y = vector.x * matrix.m[0][1] + vector.y * matrix.m[1][1] + vector.z * matrix.m[2][1] + matrix.m[3][1];

	result.z = vector.x * matrix.m[0][2] + vector.y * matrix.m[1][2] + vector.z * matrix.m[2][2] + matrix.m[3][2];

	float homogeneousCoordinate = vector.x * matrix.m[0][3] + vector.y * matrix.m[1][3] + vector.z * matrix.m[2][3] + matrix.m[3][3];

	// 透視投影の成分が0でなければ、座標を割って通常の位置へ戻す。
	if (homogeneousCoordinate != 0.0f) {
		result.x /= homogeneousCoordinate;

		result.y /= homogeneousCoordinate;

		result.z /= homogeneousCoordinate;
	}

	return result;
}

Vector3 MathUtility::TransformNormal(const Vector3& vector, const Matrix4x4& matrix) {
	// 平行移動を加えず、方向ベクトルだけを変換する。

	Vector3 result{};

	result.x =
		vector.x * matrix.m[0][0] +
		vector.y * matrix.m[1][0] +
		vector.z * matrix.m[2][0];

	result.y =
		vector.x * matrix.m[0][1] +
		vector.y * matrix.m[1][1] +
		vector.z * matrix.m[2][1];

	result.z =
		vector.x * matrix.m[0][2] +
		vector.y * matrix.m[1][2] +
		vector.z * matrix.m[2][2];

	return result;
}

//=============================================================================================//
// Matrix4x4

Matrix4x4 MathUtility::Add(const Matrix4x4& firstMatrix, const Matrix4x4& secondMatrix) {
	// 対応する成分を加算して結果を作る。
	Matrix4x4 result{};

	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			result.m[row][column] = firstMatrix.m[row][column] + secondMatrix.m[row][column];
		}
	}

	return result;
}

Matrix4x4 MathUtility::Subtract(const Matrix4x4& firstMatrix, const Matrix4x4& secondMatrix) {
	// 対応する成分を減算して結果を作る。
	Matrix4x4 result{};

	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			result.m[row][column] = firstMatrix.m[row][column] - secondMatrix.m[row][column];
		}
	}

	return result;
}

Matrix4x4 MathUtility::Multiply(const Matrix4x4& firstMatrix, const Matrix4x4& secondMatrix) {
	// 行と列の対応する成分を掛け、合計を求める。
	Matrix4x4 result{};

	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			result.m[row][column] = 0.0f;

			for (int innerIndex = 0; innerIndex < 4; ++innerIndex) {
				result.m[row][column] += firstMatrix.m[row][innerIndex] * secondMatrix.m[innerIndex][column];
			}
		}
	}

	return result;
}

Matrix4x4 MathUtility::Inverse(const Matrix4x4& matrix) {
	// 行列式と余因子から、元の変換を打ち消す逆行列を求める。
	float determinant =
		matrix.m[0][0] * matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][3]
		- matrix.m[0][0] * matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][2]
		- matrix.m[0][0] * matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][3]
		+ matrix.m[0][0] * matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][1]
		+ matrix.m[0][0] * matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][2]
		- matrix.m[0][0] * matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][1]
		- matrix.m[0][1] * matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][3]
		+ matrix.m[0][1] * matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][2]
		+ matrix.m[0][1] * matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][3]
		- matrix.m[0][1] * matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][0]
		- matrix.m[0][1] * matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][2]
		+ matrix.m[0][1] * matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][0]
		+ matrix.m[0][2] * matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][3]
		- matrix.m[0][2] * matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][1]
		- matrix.m[0][2] * matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][3]
		+ matrix.m[0][2] * matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][0]
		+ matrix.m[0][2] * matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][1]
		- matrix.m[0][2] * matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][0]
		- matrix.m[0][3] * matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][2]
		+ matrix.m[0][3] * matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][1]
		+ matrix.m[0][3] * matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][2]
		- matrix.m[0][3] * matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][0]
		- matrix.m[0][3] * matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][1]
		+ matrix.m[0][3] * matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][0];

	// 余因子を行列式で割り、逆行列の各成分を求める。
	Matrix4x4 result{};

	result.m[0][0] = (+matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][3] - matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][2] - matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][3] + matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][1] + matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][2] - matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][1]) / determinant;

	result.m[0][1] = (-matrix.m[0][1] * matrix.m[2][2] * matrix.m[3][3] + matrix.m[0][1] * matrix.m[2][3] * matrix.m[3][2] + matrix.m[0][2] * matrix.m[2][1] * matrix.m[3][3] - matrix.m[0][2] * matrix.m[2][3] * matrix.m[3][1] - matrix.m[0][3] * matrix.m[2][1] * matrix.m[3][2] + matrix.m[0][3] * matrix.m[2][2] * matrix.m[3][1]) / determinant;

	result.m[0][2] = (+matrix.m[0][1] * matrix.m[1][2] * matrix.m[3][3] - matrix.m[0][1] * matrix.m[1][3] * matrix.m[3][2] - matrix.m[0][2] * matrix.m[1][1] * matrix.m[3][3] + matrix.m[0][2] * matrix.m[1][3] * matrix.m[3][1] + matrix.m[0][3] * matrix.m[1][1] * matrix.m[3][2] - matrix.m[0][3] * matrix.m[1][2] * matrix.m[3][1]) / determinant;

	result.m[0][3] = (-matrix.m[0][1] * matrix.m[1][2] * matrix.m[2][3] + matrix.m[0][1] * matrix.m[1][3] * matrix.m[2][2] + matrix.m[0][2] * matrix.m[1][1] * matrix.m[2][3] - matrix.m[0][2] * matrix.m[1][3] * matrix.m[2][1] - matrix.m[0][3] * matrix.m[1][1] * matrix.m[2][2] + matrix.m[0][3] * matrix.m[1][2] * matrix.m[2][1]) / determinant;

	result.m[1][0] = (-matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][3] + matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][2] + matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][3] - matrix.m[1][2] * matrix.m[2][3] * matrix.m[3][0] - matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][2] + matrix.m[1][3] * matrix.m[2][2] * matrix.m[3][0]) / determinant;

	result.m[1][1] = (+matrix.m[0][0] * matrix.m[2][2] * matrix.m[3][3] - matrix.m[0][0] * matrix.m[2][3] * matrix.m[3][2] - matrix.m[0][2] * matrix.m[2][0] * matrix.m[3][3] + matrix.m[0][2] * matrix.m[2][3] * matrix.m[3][0] + matrix.m[0][3] * matrix.m[2][0] * matrix.m[3][2] - matrix.m[0][3] * matrix.m[2][2] * matrix.m[3][0]) / determinant;

	result.m[1][2] = (-matrix.m[0][0] * matrix.m[1][2] * matrix.m[3][3] + matrix.m[0][0] * matrix.m[1][3] * matrix.m[3][2] + matrix.m[0][2] * matrix.m[1][0] * matrix.m[3][3] - matrix.m[0][2] * matrix.m[1][3] * matrix.m[3][0] - matrix.m[0][3] * matrix.m[1][0] * matrix.m[3][2] + matrix.m[0][3] * matrix.m[1][2] * matrix.m[3][0]) / determinant;

	result.m[1][3] = (+matrix.m[0][0] * matrix.m[1][2] * matrix.m[2][3] - matrix.m[0][0] * matrix.m[1][3] * matrix.m[2][2] - matrix.m[0][2] * matrix.m[1][0] * matrix.m[2][3] + matrix.m[0][2] * matrix.m[1][3] * matrix.m[2][0] + matrix.m[0][3] * matrix.m[1][0] * matrix.m[2][2] - matrix.m[0][3] * matrix.m[1][2] * matrix.m[2][0]) / determinant;

	result.m[2][0] = (+matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][3] - matrix.m[1][0] * matrix.m[2][3] * matrix.m[3][1] - matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][3] + matrix.m[1][1] * matrix.m[2][3] * matrix.m[3][0] + matrix.m[1][3] * matrix.m[2][0] * matrix.m[3][1] - matrix.m[1][3] * matrix.m[2][1] * matrix.m[3][0]) / determinant;

	result.m[2][1] = (-matrix.m[0][0] * matrix.m[2][1] * matrix.m[3][3] + matrix.m[0][0] * matrix.m[2][3] * matrix.m[3][1] + matrix.m[0][1] * matrix.m[2][0] * matrix.m[3][3] - matrix.m[0][1] * matrix.m[2][3] * matrix.m[3][0] - matrix.m[0][3] * matrix.m[2][0] * matrix.m[3][1] + matrix.m[0][3] * matrix.m[2][1] * matrix.m[3][0]) / determinant;

	result.m[2][2] = (+matrix.m[0][0] * matrix.m[1][1] * matrix.m[3][3] - matrix.m[0][0] * matrix.m[1][3] * matrix.m[3][1] - matrix.m[0][1] * matrix.m[1][0] * matrix.m[3][3] + matrix.m[0][1] * matrix.m[1][3] * matrix.m[3][0] + matrix.m[0][3] * matrix.m[1][0] * matrix.m[3][1] - matrix.m[0][3] * matrix.m[1][1] * matrix.m[3][0]) / determinant;

	result.m[2][3] = (-matrix.m[0][0] * matrix.m[1][1] * matrix.m[2][3] + matrix.m[0][0] * matrix.m[1][3] * matrix.m[2][1] + matrix.m[0][1] * matrix.m[1][0] * matrix.m[2][3] - matrix.m[0][1] * matrix.m[1][3] * matrix.m[2][0] - matrix.m[0][3] * matrix.m[1][0] * matrix.m[2][1] + matrix.m[0][3] * matrix.m[1][1] * matrix.m[2][0]) / determinant;

	result.m[3][0] = (-matrix.m[1][0] * matrix.m[2][1] * matrix.m[3][2] + matrix.m[1][0] * matrix.m[2][2] * matrix.m[3][1] + matrix.m[1][1] * matrix.m[2][0] * matrix.m[3][2] - matrix.m[1][1] * matrix.m[2][2] * matrix.m[3][0] - matrix.m[1][2] * matrix.m[2][0] * matrix.m[3][1] + matrix.m[1][2] * matrix.m[2][1] * matrix.m[3][0]) / determinant;

	result.m[3][1] = (+matrix.m[0][0] * matrix.m[2][1] * matrix.m[3][2] - matrix.m[0][0] * matrix.m[2][2] * matrix.m[3][1] - matrix.m[0][1] * matrix.m[2][0] * matrix.m[3][2] + matrix.m[0][1] * matrix.m[2][2] * matrix.m[3][0] + matrix.m[0][2] * matrix.m[2][0] * matrix.m[3][1] - matrix.m[0][2] * matrix.m[2][1] * matrix.m[3][0]) / determinant;

	result.m[3][2] = (-matrix.m[0][0] * matrix.m[1][1] * matrix.m[3][2] + matrix.m[0][0] * matrix.m[1][2] * matrix.m[3][1] + matrix.m[0][1] * matrix.m[1][0] * matrix.m[3][2] - matrix.m[0][1] * matrix.m[1][2] * matrix.m[3][0] - matrix.m[0][2] * matrix.m[1][0] * matrix.m[3][1] + matrix.m[0][2] * matrix.m[1][1] * matrix.m[3][0]) / determinant;

	result.m[3][3] = (+matrix.m[0][0] * matrix.m[1][1] * matrix.m[2][2] - matrix.m[0][0] * matrix.m[1][2] * matrix.m[2][1] - matrix.m[0][1] * matrix.m[1][0] * matrix.m[2][2] + matrix.m[0][1] * matrix.m[1][2] * matrix.m[2][0] + matrix.m[0][2] * matrix.m[1][0] * matrix.m[2][1] - matrix.m[0][2] * matrix.m[1][1] * matrix.m[2][0]) / determinant;

	return result;
}

Matrix4x4 MathUtility::Transpose(const Matrix4x4& matrix) {
	// 行と列を入れ替えて転置行列を作る。
	Matrix4x4 result{};

	for (int row = 0; row < 4; ++row) {
		for (int column = 0; column < 4; ++column) {
			result.m[row][column] = matrix.m[column][row];
		}
	}

	return result;
}

Matrix4x4 MathUtility::MakeIdentity4x4() {
	// 対角成分を1にし、変換を加えない行列を作る。
	Matrix4x4 result{};

	for (int diagonalIndex = 0; diagonalIndex < 4; ++diagonalIndex) {
		result.m[diagonalIndex][diagonalIndex] = 1.0f;
	}

	return result;
}

Matrix4x4 MathUtility::MakeTranslateMatrix(const Vector3& translate) {
	// 移動量を行列の平行移動成分へ設定する。
	Matrix4x4 result = MakeIdentity4x4();

	result.m[3][0] = translate.x;

	result.m[3][1] = translate.y;

	result.m[3][2] = translate.z;

	return result;
}

Matrix4x4 MathUtility::MakeScaleMatrix(const Vector3& scale) {
	// 各軸の倍率を対角成分へ設定する。
	Matrix4x4 result{};

	result.m[0][0] = scale.x;

	result.m[1][1] = scale.y;

	result.m[2][2] = scale.z;

	result.m[3][3] = 1.0f;

	return result;
}

Matrix4x4 MathUtility::MakeRotateXMatrix(float radian) {
	// X軸まわりの回転を、正弦と余弦で行列にする。
	Matrix4x4 result = MakeIdentity4x4();

	result.m[1][1] = std::cos(radian);

	result.m[1][2] = std::sin(radian);

	result.m[2][1] = -std::sin(radian);

	result.m[2][2] = std::cos(radian);

	return result;
}

Matrix4x4 MathUtility::MakeRotateYMatrix(float radian) {
	// Y軸まわりの回転を、正弦と余弦で行列にする。
	Matrix4x4 result = MakeIdentity4x4();

	result.m[0][0] = std::cos(radian);

	result.m[0][2] = -std::sin(radian);

	result.m[2][0] = std::sin(radian);

	result.m[2][2] = std::cos(radian);

	return result;
}

Matrix4x4 MathUtility::MakeRotateZMatrix(float radian) {
	// Z軸まわりの回転を、正弦と余弦で行列にする。
	Matrix4x4 result = MakeIdentity4x4();

	result.m[0][0] = std::cos(radian);

	result.m[0][1] = std::sin(radian);

	result.m[1][0] = -std::sin(radian);

	result.m[1][1] = std::cos(radian);

	return result;
}

Matrix4x4 MathUtility::MakeAffineMatrix(const Vector3& scale, const Vector3& rotate, const Vector3& translate) {
	// 拡縮・回転・平行移動の行列を順に合成する。
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

Matrix4x4 MathUtility::MakePerspectiveFovMatrix(
		float fovY,
		float aspectRatio,
		float nearClip,
		float farClip) {
	// 画角・縦横比・奥行範囲から透視投影行列を作る。
	Matrix4x4 result{};

	float f = 1.0f / std::tanf(fovY / 2.0f);

	result.m[0][0] = f / aspectRatio;

	result.m[1][1] = f;

	result.m[2][2] = farClip / (farClip - nearClip);

	result.m[2][3] = 1.0f;

	result.m[3][2] = (-nearClip * farClip) / (farClip - nearClip);

	return result;
}

Matrix4x4 MathUtility::MakeOrthographicMatrix(
		float left,
		float top,
		float right,
		float bottom,
		float nearClip,
		float farClip) {
	// 指定範囲をクリップ空間へ写す平行投影行列を作る。
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

Matrix4x4 MathUtility::MakeViewportMatrix(
		float left,
		float top,
		float width,
		float height,
		float minDepth,
		float maxDepth) {
	// クリップ空間から画面上の位置へ変換する行列を作る。
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

bool MathUtility::IsCollision(const AABB& aabb, const Vector3& point) {
	// 点がAABBの各軸の範囲に入っているか確認する。

	if (aabb.min.x <= point.x && point.x <= aabb.max.x &&
		   aabb.min.y <= point.y && point.y <= aabb.max.y &&
		   aabb.min.z <= point.z && point.z <= aabb.max.z) {
		return true;
	}

	return false;
}
