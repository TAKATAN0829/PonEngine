#include "Camera.h"

//=============================================================================================//
// 初期化処理


void Camera::Initialize (int32_t clientWidth, int32_t clientHeight) {
	// 初期Transformと表示サイズから、カメラの行列を準備する。

	clientWidth_ = clientWidth;

	clientHeight_ = clientHeight;

	transform_.local_ = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,-30.0f}
	};

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix (
		fovY_,
		float (clientWidth_) / float (clientHeight_),
		nearClip_,
		farClip_);

	Update ();
}

//=============================================================================================//
// 更新処理


void Camera::Update () {
	// 現在のTransformと親子関係から、カメラの行列を更新する。


	transform_.UpdateMatrix ();

	Matrix4x4 worldMatrix = transform_.GetWorldMatrix ();

	viewMatrix_ = MathUtility::Inverse (worldMatrix);

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix (
		fovY_,
		float (clientWidth_) / float (clientHeight_),
		nearClip_,
		farClip_);

	viewProjectionMatrix_ = MathUtility::Multiply (viewMatrix_, projectionMatrix_);
}

// Gameビューも描画先の寸法に合わせ、縦横比による引き伸ばしを防ぐ。
void Camera::SetViewportSize(int32_t width, int32_t height) {
	// 表示サイズを保存し、必要な投影や描画基準へ反映する。
	// 有効な縦横比を作れないサイズは採用せず、現在の投影設定を保つ。
	if (width <= 0 || height <= 0) {
		return;
	}

	clientWidth_ = width;

	clientHeight_ = height;

	Update();
}

//=============================================================================================//
// Transform設定


void Camera::SetTransform (const TransformData& transform) {
	// 現在のTransformを更新する。

	transform_.local_ = transform;
}

void Camera::SetParent (Transform* parent) {
	// 親Transformの参照を更新する。

	transform_.SetParent (parent);
}

//=============================================================================================//
// Transform取得


TransformData Camera::GetTransform () const {
	// 現在のTransformを取得する。

	return transform_.local_;
}

//=============================================================================================//
// ViewProjectionMatrix取得


const Matrix4x4& Camera::GetViewProjectionMatrix () const {
	// ビューと投影を合成した行列を取得する。

	return viewProjectionMatrix_;
}

const Matrix4x4& Camera::GetViewMatrix () const {
	// カメラのビュー行列を取得する。

	return viewMatrix_;
}

const Matrix4x4& Camera::GetProjectionMatrix () const {
	// カメラの投影行列を取得する。

	return projectionMatrix_;
}
