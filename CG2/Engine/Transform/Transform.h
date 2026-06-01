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

	void SetParent(Transform* parent);

	void SetScale(const Vector3& scale);
	void SetRotate(const Vector3& rotate);
	void SetTranslate(const Vector3& translate);

	//=============================================================================================//
	// Getter

	const TransformData& GetLocalTransform() const;
	const Matrix4x4& GetWorldMatrix() const;

	Vector3 GetWorldPosition() const;

	Transform* GetParent() const;

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