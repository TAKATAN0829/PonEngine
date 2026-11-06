#include "EditorTheme.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"

#endif

//=============================================================================================//
// 初期化処理 / ImGuiスタイル


void EditorTheme::Initialize() {
#ifdef USE_IMGUI
	ImGui::StyleColorsLight();

	ImGuiStyle& style = ImGui::GetStyle();
	// 文字の大きさ
	style.FontScaleMain = 0.85f;
	// ウィンドウ内側の余白（横、縦）
	style.WindowPadding = {8,6};
	// 入力欄やボタンの内側の余白（横、縦）
	style.FramePadding = {5,3};
	// 項目同士の間隔（横、縦）
	style.ItemSpacing = {6,4};
	// 入力欄とラベルなど、同じ項目内の間隔（横、縦）
	style.ItemInnerSpacing = {4,3};
	// 表のセル内側の余白（横、縦）
	style.CellPadding = {4,3};
	// 字下げの幅
	style.IndentSpacing = 14;
	// ウィンドウの角の丸み
	style.WindowRounding = 0;
	// 子ウィンドウの角の丸み
	style.ChildRounding = 0;
	// 入力欄やボタンの角の丸み
	style.FrameRounding = 1;
	// ポップアップの角の丸み
	style.PopupRounding = 2;
	// タブの角の丸み
	style.TabRounding = 0;
	// スライダーのつまみの角の丸み
	style.GrabRounding = 1;
	// スクロールバーの角の丸み
	style.ScrollbarRounding = 2;
	// スクロールバーの太さ
	style.ScrollbarSize = 12;
	// 入力欄やボタンの枠線の太さ
	style.FrameBorderSize = 1;
	// ウィンドウの枠線の太さ
	style.WindowBorderSize = 1;
	// 子ウィンドウの枠線の太さ
	style.ChildBorderSize = 1;
	// ポップアップの枠線の太さ
	style.PopupBorderSize = 1;
	// 文字と操作中の項目には濃い色を使い、読みやすさを保つ。
	// 通常の文字色
	style.Colors[ImGuiCol_Text] = {0.055f,0.045f,0.085f,1};
	// 無効な項目の文字色
	style.Colors[ImGuiCol_TextDisabled] = {0.28f,0.25f,0.34f,1};
	// ウィンドウの背景色
	style.Colors[ImGuiCol_WindowBg] = {0.82f,0.78f,0.89f,1};
	// 子ウィンドウの背景色
	style.Colors[ImGuiCol_ChildBg] = {0.87f,0.84f,0.93f,1};
	// ポップアップの背景色
	style.Colors[ImGuiCol_PopupBg] = {0.94f,0.90f,0.96f,1};
	// 枠線の色
	style.Colors[ImGuiCol_Border] = {0.48f,0.40f,0.58f,1};
	// 枠線の影の色（透明で非表示）
	style.Colors[ImGuiCol_BorderShadow] = {0,0,0,0};
	// 入力欄などの通常時の背景色
	style.Colors[ImGuiCol_FrameBg] = {0.95f,0.92f,0.97f,1};
	// 入力欄などにマウスを重ねたときの背景色
	style.Colors[ImGuiCol_FrameBgHovered] = {0.82f,0.73f,0.94f,1};
	// 入力欄などを操作しているときの背景色
	style.Colors[ImGuiCol_FrameBgActive] = {0.68f,0.55f,0.86f,1};
	// タイトルバーの通常時の背景色
	style.Colors[ImGuiCol_TitleBg] = {0.68f,0.59f,0.82f,1};
	// 選択中のウィンドウのタイトルバーの背景色
	style.Colors[ImGuiCol_TitleBgActive] = {0.57f,0.44f,0.75f,1};
	// 折りたたんだウィンドウのタイトルバーの背景色
	style.Colors[ImGuiCol_TitleBgCollapsed] = {0.76f,0.69f,0.85f,1};
	// メニューバーの背景色
	style.Colors[ImGuiCol_MenuBarBg] = {0.78f,0.71f,0.88f,1};
	// ボタンの通常時の色
	style.Colors[ImGuiCol_Button] = {0.66f,0.83f,0.78f,1};
	// ボタンにマウスを重ねたときの色
	style.Colors[ImGuiCol_ButtonHovered] = {0.48f,0.74f,0.66f,1};
	// ボタンを押しているときの色
	style.Colors[ImGuiCol_ButtonActive] = {0.33f,0.62f,0.53f,1};
	// ツリーノードや選択項目などの通常時の背景色
	style.Colors[ImGuiCol_Header] = {0.78f,0.69f,0.91f,1};
	// ツリーノードや選択項目などにマウスを重ねたときの背景色
	style.Colors[ImGuiCol_HeaderHovered] = {0.68f,0.56f,0.85f,1};
	// ツリーノードや選択項目などを押しているときの背景色
	style.Colors[ImGuiCol_HeaderActive] = {0.57f,0.44f,0.75f,1};
	// チェックマークの色
	style.Colors[ImGuiCol_CheckMark] = {0.16f,0.37f,0.31f,1};
	// スライダーのつまみの通常時の色
	style.Colors[ImGuiCol_SliderGrab] = {0.35f,0.62f,0.54f,1};
	// スライダーのつまみを操作しているときの色
	style.Colors[ImGuiCol_SliderGrabActive] = {0.20f,0.47f,0.39f,1};
	// 区切り線の通常時の色
	style.Colors[ImGuiCol_Separator] = {0.58f,0.49f,0.69f,1};
	// サイズ変更用の区切り線にマウスを重ねたときの色
	style.Colors[ImGuiCol_SeparatorHovered] = {0.45f,0.32f,0.64f,1};
	// サイズ変更用の区切り線を操作しているときの色
	style.Colors[ImGuiCol_SeparatorActive] = {0.32f,0.20f,0.51f,1};
	// スクロールバーの背景色
	style.Colors[ImGuiCol_ScrollbarBg] = {0.78f,0.73f,0.85f,1};
	// スクロールバーのつまみの通常時の色
	style.Colors[ImGuiCol_ScrollbarGrab] = {0.57f,0.49f,0.69f,1};
	// スクロールバーのつまみにマウスを重ねたときの色
	style.Colors[ImGuiCol_ScrollbarGrabHovered] = {0.46f,0.36f,0.61f,1};
	// スクロールバーのつまみを操作しているときの色
	style.Colors[ImGuiCol_ScrollbarGrabActive] = {0.35f,0.25f,0.51f,1};
	// タブの通常時の色
	style.Colors[ImGuiCol_Tab] = {0.76f,0.69f,0.86f,1};
	// タブにマウスを重ねたときの色
	style.Colors[ImGuiCol_TabHovered] = {0.88f,0.69f,0.77f,1};
	// 選択中のタブの色
	style.Colors[ImGuiCol_TabSelected] = {0.91f,0.76f,0.83f,1};
	// フォーカスのないタブバーの未選択タブの色
	style.Colors[ImGuiCol_TabDimmed] = {0.78f,0.73f,0.84f,1};
	// フォーカスのないタブバーの選択中のタブの色
	style.Colors[ImGuiCol_TabDimmedSelected] = {0.85f,0.75f,0.83f,1};
	// 選択中のタブの上線の色（透明で非表示）
	style.Colors[ImGuiCol_TabSelectedOverline] = {0,0,0,0};
	// フォーカスのないタブバーの選択中の上線の色（透明で非表示）
	style.Colors[ImGuiCol_TabDimmedSelectedOverline] = {0,0,0,0};
	// ドッキング領域の空き部分の背景色
	style.Colors[ImGuiCol_DockingEmptyBg] = {0.66f,0.62f,0.75f,1};
	// ドッキング先を示すプレビューの色
	style.Colors[ImGuiCol_DockingPreview] = {0.45f,0.72f,0.65f,0.55f};
	// 表の見出し行の背景色
	style.Colors[ImGuiCol_TableHeaderBg] = {0.73f,0.65f,0.85f,1};
	// 表の外枠や見出しの区切り線の色
	style.Colors[ImGuiCol_TableBorderStrong] = {0.48f,0.40f,0.58f,1};
	// 表の内部の区切り線の色
	style.Colors[ImGuiCol_TableBorderLight] = {0.68f,0.61f,0.77f,1};
	// 表の行の背景色（透明）
	style.Colors[ImGuiCol_TableRowBg] = {0,0,0,0};
	// 表の交互の行に使う背景色
	style.Colors[ImGuiCol_TableRowBgAlt] = {0.69f,0.81f,0.78f,0.35f};
	// 選択した文字の背景色
	style.Colors[ImGuiCol_TextSelectedBg] = {0.49f,0.72f,0.85f,0.65f};
	
#endif
}
