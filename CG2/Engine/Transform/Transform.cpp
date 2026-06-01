#include "Transform.h"

//=============================================================================================//
// 更新処理

void Transform::UpdateMatrix() {

	Matrix4x4 localMatrix = MathUtility::MakeAffineMatrix(
		local_.scale,
		local_.rotate,
		local_.translate
	);

	if (parent_) {
		worldMatrix_ = MathUtility::Multiply(localMatrix, parent_->worldMatrix_);
	} else {
		worldMatrix_ = localMatrix;
	}
}

//=============================================================================================//
// Setter

void Transform::SetParent(Transform* parent) {

	parent_ = parent;
}

void Transform::SetScale(const Vector3& scale) {

	local_.scale = scale;
}

void Transform::SetRotate(const Vector3& rotate) {

	local_.rotate = rotate;
}

void Transform::SetTranslate(const Vector3& translate) {

	local_.translate = translate;
}

//=============================================================================================//
// Getter

const TransformData& Transform::GetLocalTransform() const {

	return local_;
}

const Matrix4x4& Transform::GetWorldMatrix() const {

	return worldMatrix_;
}

Vector3 Transform::GetWorldPosition() const {

	Vector3 result = {
		worldMatrix_.m[3][0],
		worldMatrix_.m[3][1],
		worldMatrix_.m[3][2]
	};

	return result;
}

Transform* Transform::GetParent() const {

	return parent_;
}