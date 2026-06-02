#include "Camera.h"

//=================================================================================================//
// 初期化処理

void Camera::Initialize (int32_t clientWidth, int32_t clientHeight) {

	clientWidth_ = clientWidth;

	clientHeight_ = clientHeight;

	transform_.local_ = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,-30.0f}
	};

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix (fovY_, float (clientWidth_) / float (clientHeight_), nearClip_, farClip_);

	Update ();
}

//=================================================================================================//
// 更新処理

void Camera::Update () {


	transform_.UpdateMatrix ();

	Matrix4x4 worldMatrix = transform_.GetWorldMatrix ();

	viewMatrix_ = MathUtility::Inverse (worldMatrix);

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix (fovY_, float (clientWidth_) / float (clientHeight_), nearClip_, farClip_);

	viewProjectionMatrix_ = MathUtility::Multiply (viewMatrix_, projectionMatrix_);
}

//=================================================================================================//
// Transform設定

void Camera::SetTransform (const TransformData& transform) {

	transform_.local_ = transform;
}

void Camera::SetParent (Transform* parent) {

	transform_.SetParent (parent);
}

//=================================================================================================//
// Transform取得

TransformData Camera::GetTransform () const {

	return transform_.local_;
}


//=============================================================================================//
// ViewProjectionMatrix取得

const Matrix4x4& Camera::GetViewProjectionMatrix () const {

	return viewProjectionMatrix_;
}

const Matrix4x4& Camera::GetViewMatrix () const {

	return viewMatrix_;
}

const Matrix4x4& Camera::GetProjectionMatrix () const {

	return projectionMatrix_;
}