#pragma once

// 自作
#include "MathUtility.h"
#include "EngineStructs.h"

class Transform {
public:

	//=============================================================================================//
	// 更新処理

	void UpdateMatrix();

	//=============================================================================================//
	// Setter

	void SetParent(Transform* parent) { parent_ = parent; }
	void SetScale(const Vector3& scale) { local_.scale = scale; }
	void SetRotate(const Vector3& rotate) { local_.rotate = rotate; }
	void SetTranslate(const Vector3& translate) { local_.translate = translate; }

	//=============================================================================================//
	// Getter

	const TransformData& GetLocalTransform() const { return local_; }
	const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }
	Vector3 GetWorldPosition() const { return { worldMatrix_.m[3][0], worldMatrix_.m[3][1], worldMatrix_.m[3][2] }; }
	Transform* GetParent() const { return parent_; }

public:

	TransformData local_ = {
		{1.0f, 1.0f, 1.0f},
		{0.0f, 0.0f, 0.0f},
		{0.0f, 0.0f, 0.0f}
	};

private:

	Matrix4x4 worldMatrix_ = MathUtility::MakeIdentity4x4();

	Transform* parent_ = nullptr;
};