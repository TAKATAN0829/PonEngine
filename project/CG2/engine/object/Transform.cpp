#include "Transform.h"

//=============================================================================================//
// 更新処理


void Transform::UpdateMatrix() {
	// ローカルTransformから行列を作り、必要なら親の変換を合成する。

	Matrix4x4 localMatrix = MathUtility::MakeAffineMatrix(local_.scale, local_.rotate, local_.translate);

	// 親がいればローカル行列へ親のワールド行列を掛け、階層全体の変換にする。
	if (parent_) {
		worldMatrix_ = MathUtility::Multiply(localMatrix, parent_->worldMatrix_);
	} else {
		worldMatrix_ = localMatrix;
	}
}

//=============================================================================================//
// Setter

void Transform::SetParent(Transform* parent) {
	// 親Transformの参照を更新する。
	parent_ = parent;
}

void Transform::SetScale(const Vector3& scale) {
	// ローカルTransformの拡縮を更新する。
	local_.scale = scale;
}

void Transform::SetRotate(const Vector3& rotate) {
	// ローカルTransformの回転を更新する。
	local_.rotate = rotate;
}

void Transform::SetTranslate(const Vector3& translate) {
	// ローカルTransformの位置を更新する。
	local_.translate = translate;
}

//=============================================================================================//
// Getter

const TransformData& Transform::GetLocalTransform() const {
	// 親に対するローカルTransformを取得する。
	return local_;
}

const Matrix4x4& Transform::GetWorldMatrix() const {
	// 親子変換を反映したワールド行列を取得する。
	return worldMatrix_;
}

Vector3 Transform::GetWorldPosition() const {
	// ワールド行列から求めた位置を取得する。
	return { worldMatrix_.m[3][0], worldMatrix_.m[3][1], worldMatrix_.m[3][2] };
}

Transform* Transform::GetParent() const {
	// 親として設定したTransformを取得する。
	return parent_;
}
