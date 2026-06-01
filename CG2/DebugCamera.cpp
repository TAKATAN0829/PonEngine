#include "DebugCamera.h"
#include "../externals/imgui/imgui.h"

#include <dinput.h>

void DebugCamera::Initialize(int32_t clientWidth, int32_t clientHeight) {

	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;

	transform_.scale = { 1.f,1.f,1.f };
	transform_.rotate = { 0.f,0.f,0.f };
	transform_.translate = { 0.f,0.f,-30.f };

	UpdateMatrix();
}

void DebugCamera::Update() {
	if (input_ == nullptr) {
		return;
	}
	// 移動
	if (input_->IsPressKey(DIK_W)) {
		transform_.translate.z += moveSpeed_;
	}
	if (input_->IsPressKey(DIK_A)) {
		transform_.translate.x += moveSpeed_;
	}
	if (input_->IsPressKey(DIK_S)) {
		transform_.translate.z += moveSpeed_;
	}
	if (input_->IsPressKey(DIK_D)) {
		transform_.translate.z += moveSpeed_;
	}
	if (input_->IsPressKey(DIK_Q)) {
		transform_.translate.y -= moveSpeed_;
	}
	if (input_->IsPressKey(DIK_E)) {
		transform_.translate.y += moveSpeed_;
	}

	// 回転
	if (input_->IsPressKey(DIK_UP)) {
		transform_.rotate.x -= rotateSpeed_;
	}

	if (input_->IsPressKey(DIK_DOWN)) {
		transform_.rotate.x += rotateSpeed_;
	}

	if (input_->IsPressKey(DIK_LEFT)) {
		transform_.rotate.y -= rotateSpeed_;
	}

	if (input_->IsPressKey(DIK_RIGHT)) {
		transform_.rotate.y += rotateSpeed_;
	}

#ifdef _DEBUG
	ImGui::Begin("DebugCamera");
	ImGui::DragFloat3("Translate", &transform_.translate.x, 0.1f);
	ImGui::DragFloat3("Rotate", &transform_.rotate.x, 0.01f);
	ImGui::DragFloat("MoveSpeed", &moveSpeed_, 0.01f);
	ImGui::DragFloat("RotateSpeed", &rotateSpeed_, 0.001f);
	ImGui::End();
#endif

	UpdateMatrix();
}

void DebugCamera::UpdateMatrix() {

	Matrix4x4 cameraMatrix = MathUtility::MakeAffineMatrix(
		transform_.scale,
		transform_.rotate,
		transform_.translate
	);

	viewMatrix_ = MathUtility::Inverse(cameraMatrix);

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix(
		0.45f,
		float(clientWidth_) / float(clientHeight_),
		0.1f,
		1000.0f
	);

	viewProjectionMatrix_ = MathUtility::Multiply(viewMatrix_, projectionMatrix_);
}

const Matrix4x4& DebugCamera::GetViewProjectionMatrix() const {
	return viewProjectionMatrix_;
}

const TransformData& DebugCamera::GetTransform() const {
	return transform_;
}