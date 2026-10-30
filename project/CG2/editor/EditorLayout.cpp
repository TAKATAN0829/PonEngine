#include "EditorLayout.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif

// メニューとドッキング用の親を画面全体に置き、各作業領域を分割する。
void EditorLayout::Draw() {
#ifdef USE_IMGUI
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
        if (ImGui::BeginMenu("表示 / View")) {
            if (ImGui::MenuItem("初期レイアウトに戻す")) { resetRequested_ = true; }
            ImGui::SliderFloat("文字サイズ", &ImGui::GetStyle().FontScaleMain, 0.85f, 1.5f, "%.2f");
            ImGui::EndMenu();
        }
        ImGui::EndMenuBar();
    }
    const ImGuiID dockSpaceId = ImGui::GetID("PonEngineDockspaceV2");
    if (resetRequested_ || ImGui::DockBuilderGetNode(dockSpaceId) == nullptr) {
        ImGui::DockBuilderRemoveNode(dockSpaceId);
        ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->WorkSize);
        ImGuiID mainNode = dockSpaceId;
        const ImGuiID topNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Up, 0.16f, nullptr, &mainNode);
        const ImGuiID bottomNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Down, 0.24f, nullptr, &mainNode);
        const ImGuiID leftNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Left, 0.20f, nullptr, &mainNode);
        const ImGuiID rightNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Right, 0.28f, nullptr, &mainNode);
        ImGui::DockBuilderDockWindow("Toolbar", topNode);
        ImGui::DockBuilderDockWindow("Hierarchy", leftNode);
        ImGui::DockBuilderDockWindow("World Settings", leftNode);
        ImGui::DockBuilderDockWindow("Camera", leftNode);
        ImGui::DockBuilderDockWindow("Scene", mainNode);
        ImGui::DockBuilderDockWindow("Inspector", rightNode);
        ImGui::DockBuilderDockWindow("Particle Editor", rightNode);
        ImGui::DockBuilderDockWindow("Project", bottomNode);
        ImGui::DockBuilderDockWindow("Console", bottomNode);
        ImGui::DockBuilderFinish(dockSpaceId);
        resetRequested_ = false;
    }
    ImGui::DockSpace(dockSpaceId);
    ImGui::End();
#endif
}
