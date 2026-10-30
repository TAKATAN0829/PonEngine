#include "DebugCamera.h"
#include "imgui/imgui.h"

#include <algorithm>

void DebugCamera::Initialize(int32_t clientWidth, int32_t clientHeight) {

	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;

	transform_.scale = { 1.f,1.f,1.f };
	transform_.rotate = { 0.f,0.f,0.f };
	transform_.translate = { 0.f,0.f,-30.f };

	UpdateProjectionMatrix();
	UpdateMatrix();
}

// UIやギズモが入力を使っている間は、カメラへマウス操作を渡さない。
void DebugCamera::Update() {
#ifdef USE_IMGUI
	if (!editorInputEnabled_) {
		return;
	}
	const ImGuiIO& inputState = ImGui::GetIO();
	if (inputState.WantTextInput) {
		return;
	}
	bool changed = false;
	if (ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
		if (inputState.MouseDelta.x != 0 || inputState.MouseDelta.y != 0) {
			transform_.rotate.x += inputState.MouseDelta.y * rotateSpeed_;
			transform_.rotate.x = (std::clamp)(transform_.rotate.x, -1.55f, 1.55f);
			transform_.rotate.y += inputState.MouseDelta.x * rotateSpeed_;
			changed = true;
		}
	}
	Vector2 panDelta{};
	if (ImGui::IsMouseDown(ImGuiMouseButton_Middle)) {
		panDelta = {inputState.MouseDelta.x, inputState.MouseDelta.y};
	}
	if (panDelta.x != 0 || panDelta.y != 0 || inputState.MouseWheel != 0) {
		PanAndZoom(panDelta, inputState.MouseWheel);
		changed = true;
	}
	// マウスが動かなければ逆行列も計算し直さない。
	if (changed) {
		UpdateMatrix();
	}
#endif
}

// 画面をつかんで動かす中ドラッグと、視線方向へのホイール移動を合成する。
void DebugCamera::PanAndZoom(const Vector2& mouseDelta, float wheel) {
	const Matrix4x4 rotation = MathUtility::MakeAffineMatrix({1, 1, 1}, transform_.rotate, {});
	const Vector3 movement = {-mouseDelta.x * panSpeed_, mouseDelta.y * panSpeed_, wheel * zoomSpeed_};
	transform_.translate = MathUtility::Add(transform_.translate, MathUtility::TransformNormal(movement, rotation));
}

//=============================================================================================//
// 描画処理

void DebugCamera::Draw(bool* open) {
#ifdef USE_IMGUI
	ImGui::SetNextWindowSize({350, 250}, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Camera", open)) {
		bool edited = ImGui::DragFloat3("Translate", &transform_.translate.x, 0.1f);
		if (ImGui::DragFloat3("Rotate", &transform_.rotate.x, 0.01f)) {
			edited = true;
		}
		ImGui::DragFloat("Pan Speed", &panSpeed_, 0.001f, 0.001f, 1, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Zoom Speed", &zoomSpeed_, 0.1f, 0.01f, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::DragFloat("Rotate Speed", &rotateSpeed_, 0.001f, 0.001f, 0.1f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
		if (edited) {
			UpdateMatrix();
		}
	}
	ImGui::End();
#else
	(void)open;
#endif
}

void DebugCamera::UpdateMatrix() {

	cameraMatrix_ = MathUtility::MakeAffineMatrix(transform_.scale, transform_.rotate, transform_.translate);

	viewMatrix_ = MathUtility::Inverse(cameraMatrix_);


	viewProjectionMatrix_ = MathUtility::Multiply(viewMatrix_, projectionMatrix_);
}

// 注視後すぐに行列を更新し、同じフレームの選択枠も新しいカメラに合わせる。
void DebugCamera::Focus(const Vector3& targetPosition) {
	const Vector3 forwardDirection = {cameraMatrix_.m[2][0], cameraMatrix_.m[2][1], cameraMatrix_.m[2][2]};
	transform_.translate = MathUtility::Subtract(targetPosition, MathUtility::Multiply(10.0f, forwardDirection));
	UpdateMatrix();
}

// パネルのアスペクト比に合わせ、投影とクリック判定の行列を同時に更新する。
void DebugCamera::SetViewportSize(int32_t width, int32_t height) {
	if (width <= 0 || height <= 0) {
		return;
	}
	if (clientWidth_ == width && clientHeight_ == height) {
		return;
	}
	clientWidth_ = width;
	clientHeight_ = height;
	UpdateProjectionMatrix();
	UpdateMatrix();
}

// 表示サイズが変わらない間は投影行列を再利用する。
void DebugCamera::UpdateProjectionMatrix() {
	projectionMatrix_ = MathUtility::MakePerspectiveFovMatrix(0.45f, static_cast<float>(clientWidth_) / static_cast<float>(clientHeight_), 0.1f, 1000.0f);
}
