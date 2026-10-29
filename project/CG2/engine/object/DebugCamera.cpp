#include "DebugCamera.h"
#include "imgui/imgui.h"

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

	bool allowCameraInput = true;
#ifdef USE_IMGUI
	// 名前入力やショートカットに使ったキーで、カメラまで動かないようにする。
	const ImGuiIO& inputState = ImGui::GetIO();
	if (inputState.WantCaptureKeyboard || inputState.WantTextInput || inputState.KeyCtrl) {
		allowCameraInput = false;
	}
#endif
	if (allowCameraInput) {
#ifdef USE_IMGUI
		// UI上では右ドラッグを消費せず、シーンを見ている間だけ視点を回転する。
		if (!inputState.WantCaptureMouse && ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
			transform_.rotate.x += inputState.MouseDelta.y * rotateSpeed_;
			transform_.rotate.y += inputState.MouseDelta.x * rotateSpeed_;
		}
#endif
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

		transform_.translate = MathUtility::Add(transform_.translate, velocity);
	}

#ifdef USE_IMGUI
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

	cameraMatrix_ = MathUtility::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	viewMatrix_ = MathUtility::Inverse(cameraMatrix_);

	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix(0.45f, float(clientWidth_) / float(clientHeight_), 0.1f, 1000.0f);

	viewProjectionMatrix_ = MathUtility::Multiply(viewMatrix_, projectionMatrix_);
}

// 注視後すぐに行列を更新し、同じフレームの選択枠も新しいカメラに合わせる。
void DebugCamera::Focus(const Vector3& targetPosition) {
	const Vector3 forwardDirection = {cameraMatrix_.m[2][0], cameraMatrix_.m[2][1], cameraMatrix_.m[2][2]};
	transform_.translate = MathUtility::Subtract(targetPosition, MathUtility::Multiply(10.0f, forwardDirection));
	UpdateMatrix();
}
