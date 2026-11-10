#include "EditorLayout.h"
#include "FrameProfiler.h"
#include <cstdio>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#endif

//=============================================================================================//
// 描画処理


// メニューとドッキング用の親を画面全体に置き、各作業領域を分割する。
EditorMenuAction EditorLayout::Begin(bool isSceneAvailable, bool isPlaying, bool hasUnsavedChanges, EditorToolVisibility& visibility) {
	EditorMenuAction action = EditorMenuAction::None;

#ifdef USE_IMGUI

	// メイン表示領域を取得。
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	// 次のウィンドウ位置を設定。
	ImGui::SetNextWindowPos(viewport->WorkPos);
	// 次のウィンドウサイズを設定。
	ImGui::SetNextWindowSize(viewport->WorkSize);
	// 次の表示先を設定。
	ImGui::SetNextWindowViewport(viewport->ID);

	const ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoTitleBar |
	ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
	ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_MenuBar;

	// スタイル値を適用。
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, {0,0});
	// スタイル値を適用。
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0);
	// 「PonEngine Workspace」ウィンドウを開始。
	ImGui::Begin("PonEngine Workspace", nullptr, flags);
	// スタイル値を戻す。
	ImGui::PopStyleVar(2);
	// メニューバーを開始。
	if (ImGui::BeginMenuBar()) {
		// 「PonEngine」テキストを表示。
		ImGui::TextUnformatted("PonEngine");

		// Sceneの作成・保存・読込操作をまとめ、実行中は変更操作を止める。
		// 「シーン / Scene」メニューを開始。
		if (ImGui::BeginMenu("シーン / Scene")) {
			// 操作無効化を開始。
			ImGui::BeginDisabled(!isSceneAvailable || isPlaying);
			// 「新規」メニュー選択を受け取る。
			if (ImGui::MenuItem("新規", "Ctrl+N")) {
				action = EditorMenuAction::NewScene;
			}

			// 「保存」メニュー選択を受け取る。
			if (ImGui::MenuItem("保存", "Ctrl+S")) {
				action = EditorMenuAction::SaveScene;
			}

			// 「名前を付けて保存...」メニュー選択を受け取る。
			if (ImGui::MenuItem("名前を付けて保存...", "Ctrl+Shift+S")) {
				action = EditorMenuAction::SaveSceneAs;
			}

			// 「開く...」メニュー選択を受け取る。
			if (ImGui::MenuItem("開く...", "Ctrl+O")) {
				action = EditorMenuAction::LoadScene;
			}

			// 操作無効化を終了。
			ImGui::EndDisabled();
			// メニューを終了。
			ImGui::EndMenu();
		}

		if (hasUnsavedChanges) {
			// 明るいメニューバーでも読める、濃い赤で未保存を示す。
			// 「* 未保存」色付きテキストを表示。
			ImGui::TextColored({0.45f,0.035f,0.055f,1}, "* 未保存");
		}

		// 作業レイアウトの初期化と、各補助パネルの表示状態を切り替える。
		// 「表示 / View」メニューを開始。
		if (ImGui::BeginMenu("表示 / View")) {
			// 「初期レイアウトに戻す」メニュー選択を受け取る。
			if (ImGui::MenuItem("初期レイアウトに戻す")) {
				isResetRequested_ = true;

				visibility = {};
			}

			// 区切り線を表示。
			ImGui::Separator();
			// 「補助画面 / Tools」補助テキストを表示。
			ImGui::TextDisabled("補助画面 / Tools");
			// 「Particle Editor」メニュー選択を受け取る。
			ImGui::MenuItem("Particle Editor", nullptr, &visibility.isParticleEditorVisible);
			// 「World Settings」メニュー選択を受け取る。
			ImGui::MenuItem("World Settings", nullptr, &visibility.areWorldSettingsVisible);
			// 「Camera」メニュー選択を受け取る。
			ImGui::MenuItem("Camera", nullptr, &visibility.isCameraVisible);
			// 「Performance / パフォーマンス」メニュー選択を受け取る。
			ImGui::MenuItem("Performance / パフォーマンス", nullptr, &visibility.isPerformanceVisible);
			// メニューを終了。
			ImGui::EndMenu();
		}

		// メニューバーを終了。
		ImGui::EndMenuBar();
	}

	// メニューの下をドッキング領域にして、未作成時は標準配置を組み立てる。
	// 「PonEngineDockspaceV5」項目IDを取得。
	const ImGuiID dockSpaceId = ImGui::GetID("PonEngineDockspaceV5");

	if (isResetRequested_) {
		// ドッキング領域を削除。
		ImGui::DockBuilderRemoveNode(dockSpaceId);
	}

	// ドッキング領域を取得。
	if (ImGui::DockBuilderGetNode(dockSpaceId) == nullptr) {
		// ドッキング領域を削除。
		ImGui::DockBuilderRemoveNode(dockSpaceId);
		// ドッキング領域を作成。
		ImGui::DockBuilderAddNode(dockSpaceId, ImGuiDockNodeFlags_DockSpace);
		// ドッキング領域サイズを設定。
		ImGui::DockBuilderSetNodeSize(dockSpaceId, viewport->WorkSize);

		ImGuiID mainNode = dockSpaceId;

		// ドッキング領域を分割。
		const ImGuiID bottomNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Down, 0.23f, nullptr, &mainNode);
		// ドッキング領域を分割。
		const ImGuiID leftNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Left, 0.165f, nullptr, &mainNode);
		// ドッキング領域を分割。
		const ImGuiID rightNode = ImGui::DockBuilderSplitNode(mainNode, ImGuiDir_Right, 0.29f, nullptr, &mainNode);
		// 「Hierarchy」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Hierarchy", leftNode);

		// 外側のViewportタブを隠し、中のSceneView/GameViewだけを表示する。
		// ドッキング領域を取得。
		ImGui::DockBuilderGetNode(mainNode)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;
		// 「Scene Viewport」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Scene Viewport", mainNode);
		// 「Inspector」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Inspector", rightNode);
		// 「Project」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Project", bottomNode);
		// 「Console」ウィンドウを配置。
		ImGui::DockBuilderDockWindow("Console", bottomNode);
		// ドッキング配置を確定。
		ImGui::DockBuilderFinish(dockSpaceId);

		isResetRequested_ = false;
	}

	// 空のUI項目を挟まず、再生バーの上余白だけを2px確保する。
	// 縦位置を設定。
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2);

#else

	// 変数の値を使わずに捨てる
	// ここでの(void)は”捨てる”という意味
	(void)isSceneAvailable;

	(void)isPlaying;

	(void)hasUnsavedChanges;

	(void)visibility;

#endif
	return action;
}

// 再生操作の下を作業領域として使い、専用Toolbarタブは作らない。
void EditorLayout::Draw(const std::string& status) {
#ifdef USE_IMGUI
	// 「PonEngineDockspaceV5」項目IDを取得。
	const ImGuiID dockSpaceId = ImGui::GetID("PonEngineDockspaceV5");

	// 結果通知は再生バーに混在させず、画面下部へ一行で表示する。
	// テキスト行の高さを取得。
	const float statusHeight = ImGui::GetTextLineHeightWithSpacing() + 4;
	// 残り表示領域を取得。
	ImVec2 dockSize = ImGui::GetContentRegionAvail();

	dockSize.y -= statusHeight;

	if (dockSize.y < 1) {
		dockSize.y = 1;
	}

	// 中央は内側の2タブで切り替えるため、別の画面が重なって隠れないようにする。
	// ドッキング領域を表示。
	ImGui::DockSpace(dockSpaceId, dockSize, ImGuiDockNodeFlags_NoDockingOverCentralNode);
	// 画面位置を取得。
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	// 残り表示領域を取得。
	const float width = ImGui::GetContentRegionAvail().x;
	// 描画リストを取得。
	ImGui::GetWindowDrawList()->AddRectFilled(origin,{origin.x+width,origin.y+statusHeight},ImGui::GetColorU32(ImGuiCol_MenuBarBg));

	char statistics[96] = "FPS --";

	// 最新の計測結果を使い、GPU時間が有効なフレームだけGPUの値を表示する。
	const FrameProfiler::Sample& sample = FrameProfiler::GetLatest();

	if (sample.frameMilliseconds > 0) {
		if (sample.isGpuTimingValid) {
			std::snprintf(statistics,sizeof(statistics),"%.0f FPS  |  %.2f ms  |  GPU %.2f ms",1000.0f/sample.frameMilliseconds,sample.frameMilliseconds,sample.gpuMilliseconds);
		} else {
			std::snprintf(statistics,sizeof(statistics),"%.0f FPS  |  %.2f ms",1000.0f/sample.frameMilliseconds,sample.frameMilliseconds);
		}
	}

	// CalcTextSizeを処理。
	const float statisticsWidth = ImGui::CalcTextSize(statistics).x;

	float messageWidth = width-16;

	// 通知を表示できる幅を残せる場合だけ、右端に計測値を出す。
	const bool shouldShowStatistics = width > statisticsWidth+120;

	if (shouldShowStatistics) {
		messageWidth -= statisticsWidth+20;
	}

	// 画面位置を設定。
	ImGui::SetCursorScreenPos({origin.x+8,origin.y+2});

	// 長い通知を計測値の手前で切り、全文はマウスを重ねたときに表示する。
	// 描画範囲を制限。
	ImGui::PushClipRect(origin,{origin.x+messageWidth,origin.y+statusHeight},true);

	if (status.empty()) {
		// 「Ready」補助テキストを表示。
		ImGui::TextDisabled("Ready");
	} else {
		// テキストを表示。
		ImGui::TextUnformatted(status.c_str());
	}

	// 描画範囲制限を解除。
	ImGui::PopClipRect();

	if (!status.empty()) {
		// ホバー状態を確認。
		if (ImGui::IsItemHovered()) {
			// 補足説明を表示。
			ImGui::SetTooltip("%s",status.c_str());
		}
	}

	if (shouldShowStatistics) {
		// 画面位置を設定。
		ImGui::SetCursorScreenPos({origin.x+width-statisticsWidth-8,origin.y+2});
		// 補助テキストを表示。
		ImGui::TextDisabled("%s",statistics);
	}

	// ウィンドウを終了。
	ImGui::End();

#else
	(void)status;

#endif
}
