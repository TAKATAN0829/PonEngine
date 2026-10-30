#include "EditorLayout.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif

// メニューとドッキング用の親を画面全体に置き、各作業領域を分割する。
EditorMenuAction EditorLayout::Begin(bool sceneAvailable, bool playing, bool dirty, EditorToolVisibility& visibility) {
	EditorMenuAction action = EditorMenuAction::None;
#ifdef USE_IMGUI
	// プロジェクト共通の読みやすい倍率に固定する。
	ImGui::GetStyle().FontScaleMain = 0.85f;
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(viewport->WorkPos);
	ImGui::SetNextWindowSize(viewport->WorkSize);
	ImGui::SetNextWindowViewport(viewport->ID);
	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
	ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	ImGui::Begin("PonEngine Workspace", nullptr, flags);
	ImGui::PopStyleVar(2);
	if (ImGui::BeginMenuBar()) {
		ImGui::TextUnformatted("PonEngine");
		if (ImGui::BeginMenu("シーン / Scene")) {
			ImGui::BeginDisabled(!sceneAvailable || playing);
			if (ImGui::MenuItem("新規", "Ctrl+N")) {
				action = EditorMenuAction::NewScene;
			}
			if (ImGui::MenuItem("保存", "Ctrl+S")) {
				action = EditorMenuAction::SaveScene;
			}
			if (ImGui::MenuItem("読込", "Ctrl+O")) {
				action = EditorMenuAction::LoadScene;
			}
			ImGui::EndDisabled();
			ImGui::EndMenu();
		}
		if (dirty) {
			ImGui::TextColored({1,0.72f,0.3f,1}, "* 未保存");
		}
		if (ImGui::BeginMenu("表示 / View")) {
			if (ImGui::MenuItem("初期レイアウトに戻す")) {
				resetRequested_ = true;
				visibility = {};
			}
			ImGui::Separator();
			ImGui::TextDisabled("補助画面 / Tools");
			ImGui::MenuItem("Particle Editor", nullptr, &visibility.particleEditor);
			ImGui::MenuItem("World Settings", nullptr, &visibility.worldSettings);
			ImGui::MenuItem("Camera", nullptr, &visibility.camera);
			ImGui::MenuItem("Performance / パフォーマンス", nullptr, &visibility.performance);
			ImGui::EndMenu();
		}
		ImGui::EndMenuBar();
	}
	const ImGuiID dockSpaceId = ImGui::GetID("PonEngineDockspaceV4");
	if (resetRequested_) {
		ImGui::DockBuilderRemoveNode(dockSpaceId);
	}
	if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr) {
		ImGui::DockBuilderRemoveNode(dockSpaceId);
		ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->WorkSize);
		ImGuiID mainNode = dockSpaceId;
		const ImGuiID bottomNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Down, 0.24f, nullptr, &mainNode);
		const ImGuiID leftNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Left, 0.20f, nullptr, &mainNode);
		const ImGuiID rightNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Right, 0.28f, nullptr, &mainNode);
		ImGui::DockBuilderDockWindow("Hierarchy", leftNode);
		// 外側のViewportタブを隠し、中のSceneView/GameViewだけを表示する。
		ImGui::DockBuilderGetNode(mainNode)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		ImGui::DockBuilderDockWindow("Scene Viewport", mainNode);
		ImGui::DockBuilderDockWindow("Inspector", rightNode);
		ImGui::DockBuilderDockWindow("Project", bottomNode);
		ImGui::DockBuilderDockWindow("Console", bottomNode);
		ImGui::DockBuilderFinish(dockSpaceId);
		resetRequested_ = false;
	}
	// 空のUI項目を挟まず、再生バーの上余白だけを2px確保する。
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);
#else
	(void)sceneAvailable;
	(void)playing;
	(void)dirty;
	(void)visibility;
#endif
	return action;
}

// 再生操作の下を作業領域として使い、専用Toolbarタブは作らない。
void EditorLayout::Draw(const std::string& status) {
#ifdef USE_IMGUI
	const ImGuiID dockSpaceId = ImGui::GetID("PonEngineDockspaceV4");
	// 結果通知は再生バーに混在させず、画面下部へ一行で表示する。
	const float statusHeight = ImGui::GetTextLineHeightWithSpacing() + 4;
	ImVec2 dockSize = ImGui::GetContentRegionAvail();
	dockSize.y -= statusHeight;
	if (dockSize.y < 1) {
		dockSize.y = 1;
	}
	// 中央は内側の2タブで切り替えるため、別の画面が重なって隠れないようにする。
	ImGui::DockSpace(dockSpaceId, dockSize, ImGuiDockNodeFlags_NoDockingOverCentralNode);
	ImGui::TextUnformatted(status.c_str());
	ImGui::End();
#else
	(void)status;
#endif
}
