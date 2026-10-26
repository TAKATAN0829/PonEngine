#include "Transform.h"

//=============================================================================================//
// 更新処理

void Transform::UpdateMatrix() {

	Matrix4x4 localMatrix = MathUtility::MakeAffineMatrix(local_.scale, local_.rotate, local_.translate);

	if (parent_) {
		worldMatrix_ = MathUtility::Multiply(localMatrix, parent_->worldMatrix_);
	} else {
		worldMatrix_ = localMatrix;
	}
}