#include "Camera.h"

//=================================================================================================//
// 初期化処理

void Camera::Initialize (int32_t clientWidth, int32_t clientHeight) {

	clientWidth_ = clientWidth;

	clientHeight_ = clientHeight;

	transform_ = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,-5.0f}
	};

	viewMatrix_ = MathUtility::MakeIdentity4x4 ();

	projectionMatrix_ = MathUtility::MakeIdentity4x4 ();

	viewProjectionMatrix_ = MathUtility::MakeIdentity4x4 ();

	Update ();
}

//=================================================================================================//
// 更新処理

void Camera::Update () {

	Matrix4x4 cameraMatrix = MathUtility::MakeAffineMatrix (transform_.scale, transform_.rotate, transform_.translate);

	viewMatrix_ =MathUtility::Inverse (cameraMatrix);

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix (0.45f, float (clientWidth_) / float (clientHeight_), 0.1f, 100.0f);

	viewProjectionMatrix_ = MathUtility::Multiply (viewMatrix_, projectionMatrix_);
}

//=================================================================================================//
// Transform設定

void Camera::SetTransform (const TransformData& transform) {

	transform_ = transform;
}

//=================================================================================================//
// Transform取得

TransformData Camera::GetTransform () const {

	return transform_;
}

//=================================================================================================//
// ViewMatrix取得

const Matrix4x4& Camera::GetViewMatrix () const {

	return viewMatrix_;
}

//=================================================================================================//
// ProjectionMatrix取得

const Matrix4x4& Camera::GetProjectionMatrix () const {

	return projectionMatrix_;
}

//=================================================================================================//
// ViewProjectionMatrix取得

const Matrix4x4& Camera::GetViewProjectionMatrix () const {

	return viewProjectionMatrix_;
}