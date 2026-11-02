#include "SceneViewPanel.h"
#include "../game_view/GameViewPanel.h"
#include "FrameProfiler.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// タブ・画像領域・操作の呼出し順を管理する。個々の描画・操作は担当ファイルへ分ける。
void SceneViewPanel::Initialize() {
	ResetInteraction();
	dragStartPosition_ = {};
	dragAxis_ = {};
	dragStartDistance_ = 0;
	forceScene_ = false;
}

void SceneViewPanel::ResetInteraction() {
	dragging_ = false;
	draggedObject_ = kInvalidWorldObjectId;
	dragObjects_.clear();
}

// 共通の描画先とタブ切替を管理し、表示内容は各タブの実装へ任せる。
void SceneViewPanel::Draw(IEditableWorld& scene, EditorSelection& selection) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::SceneView);
#ifdef USE_IMGUI
	scene.SetEditorCameraInput(false);
	if (!ImGui::Begin("Scene Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		ResetInteraction();
		ImGui::End();
		return;
	}
	if (ImGui::BeginTabBar("SceneViewTabs")) {
		ImGuiTabItemFlags sceneFlags = ImGuiTabItemFlags_None;
		if (forceScene_) {
			sceneFlags = ImGuiTabItemFlags_SetSelected;
		}
		if (ImGui::BeginTabItem("SceneView", nullptr, sceneFlags)) {
			gameView_ = false;
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("GameView")) {
			gameView_ = true;
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
		forceScene_ = false;
	}
	if (gameView_) {
		if (dragging_) {
			CancelDrag(scene);
		}
		GameViewPanel::Draw(scene,textureHandle_,width_,height_);
	} else {
		DrawScene(scene,selection);
	}
	ImGui::End();
#else
	(void)scene;
	(void)selection;
#endif
}
