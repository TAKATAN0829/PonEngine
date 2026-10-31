#include "EditorTheme.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void EditorTheme::Initialize() {
#ifdef USE_IMGUI
	ImGui::StyleColorsDark();
	ImGuiStyle& style = ImGui::GetStyle();
	style.FontScaleMain = 0.85f;
	style.WindowPadding = {8,6};
	style.FramePadding = {5,3};
	style.ItemSpacing = {6,4};
	style.ItemInnerSpacing = {4,3};
	style.CellPadding = {4,3};
	style.IndentSpacing = 14;
	style.WindowRounding = 0;
	style.ChildRounding = 0;
	style.FrameRounding = 1;
	style.PopupRounding = 2;
	style.TabRounding = 0;
	style.GrabRounding = 1;
	style.ScrollbarRounding = 2;
	style.ScrollbarSize = 12;
	style.FrameBorderSize = 1;
	style.WindowBorderSize = 1;
	style.ChildBorderSize = 1;
	style.PopupBorderSize = 1;
	// このエンジンのsRGB描画先に合わせ、背景には低輝度の無彩色を使う。
	style.Colors[ImGuiCol_Text] = {0.80f,0.80f,0.80f,1};
	style.Colors[ImGuiCol_TextDisabled] = {0.30f,0.30f,0.30f,1};
	style.Colors[ImGuiCol_WindowBg] = {0.038f,0.038f,0.038f,1};
	style.Colors[ImGuiCol_ChildBg] = {0.030f,0.030f,0.030f,1};
	style.Colors[ImGuiCol_PopupBg] = {0.055f,0.055f,0.055f,1};
	style.Colors[ImGuiCol_Border] = {0.012f,0.012f,0.012f,1};
	style.Colors[ImGuiCol_BorderShadow] = {0,0,0,0};
	style.Colors[ImGuiCol_FrameBg] = {0.022f,0.022f,0.022f,1};
	style.Colors[ImGuiCol_FrameBgHovered] = {0.038f,0.038f,0.038f,1};
	style.Colors[ImGuiCol_FrameBgActive] = {0.055f,0.055f,0.055f,1};
	style.Colors[ImGuiCol_TitleBg] = {0.050f,0.050f,0.050f,1};
	style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_TitleBg];
	style.Colors[ImGuiCol_TitleBgCollapsed] = style.Colors[ImGuiCol_TitleBg];
	style.Colors[ImGuiCol_MenuBarBg] = {0.050f,0.050f,0.050f,1};
	style.Colors[ImGuiCol_Button] = {0.070f,0.070f,0.070f,1};
	style.Colors[ImGuiCol_ButtonHovered] = {0.105f,0.105f,0.105f,1};
	style.Colors[ImGuiCol_ButtonActive] = {0.045f,0.080f,0.120f,1};
	style.Colors[ImGuiCol_Header] = {0.065f,0.065f,0.065f,1};
	style.Colors[ImGuiCol_HeaderHovered] = {0.090f,0.090f,0.090f,1};
	style.Colors[ImGuiCol_HeaderActive] = {0.070f,0.100f,0.135f,1};
	style.Colors[ImGuiCol_CheckMark] = {0.18f,0.34f,0.52f,1};
	style.Colors[ImGuiCol_SliderGrab] = {0.16f,0.25f,0.36f,1};
	style.Colors[ImGuiCol_SliderGrabActive] = {0.24f,0.38f,0.55f,1};
	style.Colors[ImGuiCol_Separator] = {0.012f,0.012f,0.012f,1};
	style.Colors[ImGuiCol_SeparatorHovered] = {0.12f,0.16f,0.21f,1};
	style.Colors[ImGuiCol_SeparatorActive] = {0.16f,0.25f,0.36f,1};
	style.Colors[ImGuiCol_ScrollbarBg] = {0.025f,0.025f,0.025f,1};
	style.Colors[ImGuiCol_ScrollbarGrab] = {0.085f,0.085f,0.085f,1};
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = {0.12f,0.12f,0.12f,1};
	style.Colors[ImGuiCol_ScrollbarGrabActive] = {0.16f,0.16f,0.16f,1};
	style.Colors[ImGuiCol_Tab] = {0.055f,0.055f,0.055f,1};
	style.Colors[ImGuiCol_TabHovered] = style.Colors[ImGuiCol_Tab];
	style.Colors[ImGuiCol_TabSelected] = style.Colors[ImGuiCol_Tab];
	style.Colors[ImGuiCol_TabDimmed] = style.Colors[ImGuiCol_Tab];
	style.Colors[ImGuiCol_TabDimmedSelected] = style.Colors[ImGuiCol_Tab];
	style.Colors[ImGuiCol_TabSelectedOverline] = {0,0,0,0};
	style.Colors[ImGuiCol_TabDimmedSelectedOverline] = {0,0,0,0};
	style.Colors[ImGuiCol_DockingEmptyBg] = {0.022f,0.022f,0.022f,1};
	style.Colors[ImGuiCol_DockingPreview] = {0.10f,0.20f,0.32f,0.4f};
	style.Colors[ImGuiCol_TableHeaderBg] = {0.055f,0.055f,0.055f,1};
	style.Colors[ImGuiCol_TableBorderStrong] = style.Colors[ImGuiCol_Border];
	style.Colors[ImGuiCol_TableBorderLight] = {0.025f,0.025f,0.025f,1};
	style.Colors[ImGuiCol_TableRowBg] = {0,0,0,0};
	style.Colors[ImGuiCol_TableRowBgAlt] = {0.055f,0.055f,0.055f,0.5f};
	style.Colors[ImGuiCol_TextSelectedBg] = {0.12f,0.22f,0.34f,0.65f};
#endif
}
