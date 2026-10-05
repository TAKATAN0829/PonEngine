#include "DebugCamera.h"
#include "../../../externals/imgui/imgui.h"

#include <dinput.h>

void DebugCamera::Initialize(int32_t clientWidth, int32_t clientHeight) {

	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;

	transform_.scale = { 1.f,1.f,1.f };
	transform_.rotate = { 0.f,0.f,0.f };
	transform_.translate = { 0.f,0.f,-30.f };

	UpdateMatrix();
}

void DebugCamera::Update(Input* input) {
	input_ = input;

	Vector3 move{ 0.0f, 0.0f, 0.0f };

	if (input->IsPressKey(DIK_W)) {
		move.z += 1.0f;
	}

	if (input->IsPressKey(DIK_S)) {
		move.z -= 1.0f;
	}

	if (input->IsPressKey(DIK_A)) {
		move.x -= 1.0f;
	}

	if (input->IsPressKey(DIK_D)) {
		move.x += 1.0f;
	}

	if (input->IsPressKey(DIK_Q)) {
		move.y -= 1.0f;
	}

	if (input->IsPressKey(DIK_E)) {
		move.y += 1.0f;
	}

	//=============================================================================================//
	// カメラの向きに合わせて移動

	if (MathUtility::Length(move) != 0.0f) {

		move = MathUtility::Normalize(move);

		Matrix4x4 rotateMatrix =
			MathUtility::Multiply(MathUtility::MakeRotateXMatrix(transform_.rotate.x),
				MathUtility::Multiply(MathUtility::MakeRotateYMatrix(transform_.rotate.y), MathUtility::MakeRotateZMatrix(transform_.rotate.z))
			);

		Vector3 velocity = MathUtility::TransformNormal(move, rotateMatrix);

		velocity.x *= moveSpeed_;
		velocity.y *= moveSpeed_;
		velocity.z *= moveSpeed_;

		transform_.translate = MathUtility::Add(transform_.translate,velocity);
	}

#ifdef USE_IMGUI
	ImGui::Begin("DebugCamera");
	ImGui::DragFloat3(
		"Translate",
		&transform_.translate.x,
		0.1f);
	ImGui::DragFloat3(
		"Rotate",
		&transform_.rotate.x,
		0.01f);
	ImGui::DragFloat(
		"MoveSpeed",
		&moveSpeed_,
		0.01f);
	ImGui::DragFloat(
		"RotateSpeed",
		&rotateSpeed_,
		0.001f);
	ImGui::End();
#endif

	UpdateMatrix();
}

void DebugCamera::UpdateMatrix() {

	Matrix4x4 cameraMatrix = MathUtility::MakeAffineMatrix(
		transform_.scale,
		transform_.rotate,
		transform_.translate);

	viewMatrix_ = MathUtility::Inverse(cameraMatrix);

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix(
		0.45f,
		float(clientWidth_) / float(clientHeight_),
		0.1f,
		1000.0f);

	viewProjectionMatrix_ = MathUtility::Multiply(viewMatrix_, projectionMatrix_);
}

const Matrix4x4& DebugCamera::GetViewProjectionMatrix() const {
	return viewProjectionMatrix_;
}

const TransformData& DebugCamera::GetTransform() const {
	return transform_;
}