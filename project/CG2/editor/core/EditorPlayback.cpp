#include "Editor.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"

#ifdef USE_IMGUI

//=============================================================================================//
// 実装内補助定義

namespace {
	enum class PlaybackIcon { Play, Stop, Pause, Step };

	// フォントの収録文字や画像ファイルに依存しない図形アイコンを描く。
	bool DrawPlaybackButton (const char* identifier, PlaybackIcon icon, const char* hint, bool isActive) {
		// 再生状態に応じた図形アイコンと操作ヒントを表示する。
		if (isActive) {
			ImGui::PushStyleColor (ImGuiCol_Button, { 0.045f,0.10f,0.17f,1 });
		}

		const bool wasClicked = ImGui::Button (identifier, { 30,22 });

		if (isActive) {
			ImGui::PopStyleColor ();
		}

		const ImVec2 minimum = ImGui::GetItemRectMin ();
		const ImVec2 center = { minimum.x + 15, minimum.y + 11 };

		const ImU32 color = ImGui::GetColorU32 (ImGuiCol_Text);

		ImDrawList* drawList = ImGui::GetWindowDrawList ();

		if (icon == PlaybackIcon::Play || icon == PlaybackIcon::Step) {
			drawList->AddTriangleFilled ({ center.x - 5,center.y - 7 }, { center.x - 5,center.y + 7 }, { center.x + 6,center.y }, color);
		}

		if (icon == PlaybackIcon::Stop) {
			drawList->AddRectFilled ({ center.x - 6,center.y - 6 }, { center.x + 6,center.y + 6 }, color);
		}

		if (icon == PlaybackIcon::Pause) {
			drawList->AddRectFilled ({ center.x - 6,center.y - 7 }, { center.x - 2,center.y + 7 }, color);
			drawList->AddRectFilled ({ center.x + 2,center.y - 7 }, { center.x + 6,center.y + 7 }, color);
		}

		if (icon == PlaybackIcon::Step) {
			drawList->AddRectFilled ({ center.x + 7,center.y - 7 }, { center.x + 10,center.y + 7 }, color);
		}

		if (ImGui::IsItemHovered (ImGuiHoveredFlags_AllowWhenDisabled)) {
			ImGui::SetTooltip ("%s", hint);
		}

		return wasClicked;
	}
}

#endif

#endif

//=============================================================================================//
// パネル描画


// 再生・停止・一時停止・ステップだけを中央へ並べる。
void Editor::DrawToolbar (IEditableWorld& scene) {
#ifdef USE_IMGUI

	// シーン管理はメニューへ分離し、中央には再生操作だけを置く。
	// この行だけ縦の項目間隔を詰め、下の作業領域を広く保つ。
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, 2});

	// ボタン3個とその間の隙間2つを含めた幅で、操作列を中央に置く。
	const float controlsWidth = 3 * 30.0f + 2 * ImGui::GetStyle ().ItemSpacing.x;
	const float availableWidth = ImGui::GetContentRegionAvail ().x;
	const float toolbarStart = ImGui::GetCursorPosX();

	const ImVec2 toolbarOrigin = ImGui::GetCursorScreenPos();

	ImGui::GetWindowDrawList()->AddRectFilled(toolbarOrigin,{toolbarOrigin.x+availableWidth,toolbarOrigin.y+24},ImGui::GetColorU32(ImGuiCol_MenuBarBg));

	ImGui::SetCursorPosX(toolbarStart+10);

	ImGui::AlignTextToFramePadding();

	std::string displayName = document_.GetName();

	// 再生中に遷移した場合は、いま表示している場面の名前を出す。
	if (session_.IsPlaying()) {
		if (scene_ != playOrigin_) {
			displayName = sceneName_;
		}
	}

	ImGui::TextDisabled("Scene  /  %s", displayName.c_str());

	if (ImGui::IsItemHovered()) {
		if (document_.HasPath()) {
			ImGui::SetTooltip("%s", AssetPathChange::Text(document_.GetPath()).c_str());
		}
	}

	ImGui::SameLine();

	if (availableWidth > controlsWidth) {
		ImGui::SetCursorPosX (toolbarStart + (availableWidth - controlsWidth) * 0.5f);
	}

	PlaybackIcon playIcon = PlaybackIcon::Play;

	const char* playHint = "再生 (Ctrl+P)";

	if (session_.IsPlaying ()) {
		playIcon = PlaybackIcon::Stop;

		playHint = "停止 (Ctrl+P)";
	}

	if (DrawPlaybackButton ("##PlayStop", playIcon, playHint, session_.IsPlaying ())) {
		TogglePlay (scene);
	}

	ImGui::SameLine ();

	ImGui::BeginDisabled (!session_.IsPlaying ());

	if (DrawPlaybackButton ("##Pause", PlaybackIcon::Pause, "一時停止 / 再開", session_.IsPaused ())) {
		session_.TogglePause ();
	}

	ImGui::EndDisabled ();

	ImGui::SameLine ();

	// ステップは一時停止中の再生に対してだけ予約できる。
	ImGui::BeginDisabled (!session_.IsPlaying () || !session_.IsPaused ());

	if (DrawPlaybackButton ("##Step", PlaybackIcon::Step, "1フレーム進める", false)) {
		session_.RequestStep ();
	}

	ImGui::EndDisabled ();

	if (availableWidth > 440) {
		ImGui::SameLine();

		ImGui::SetCursorPosX(toolbarStart+availableWidth-90);

		const char* mode = "EDIT MODE";

		if (session_.IsPlaying()) {
			mode = "PLAY MODE";

			if (session_.IsPaused()) {
				mode = "PAUSED";
			}
		}

		ImGui::TextDisabled("%s",mode);
	}

	ImGui::PopStyleVar();

#else
	(void) scene;

#endif
}

// 保存成功または破棄の選択が済むまで、シーンを置き換えない。
void Editor::HandleShortcuts(IEditableWorld& scene) {
#ifdef USE_IMGUI

	// 文字入力・ウィジェット操作・Scene操作・ダイアログはそれぞれ独立して確認する。
	if (ImGui::GetIO().WantTextInput) {
		return;
	}

	if (ImGui::IsAnyItemActive()) {
		return;
	}

	if (sceneView_.IsManipulating()) {
		return;
	}

	if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
		return;
	}

	if (ImGui::GetIO().KeyCtrl) {
		if (ImGui::IsKeyPressed(ImGuiKey_P, false)) {
			TogglePlay(scene);
		}
	}

	if (session_.IsPlaying()) {
		return;
	}

	// 修飾キーを先に判定し、その内側で操作するキーを分類する。
	if (ImGui::GetIO().KeyCtrl) {
		if (ImGui::IsKeyPressed(ImGuiKey_N, false)) {
			RequestSceneAction(SceneAction::NewScene, scene);
		}

		if (ImGui::IsKeyPressed(ImGuiKey_O, false)) {
			OpenSceneDialog();
		}

		if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			if (ImGui::GetIO().KeyShift) {
				OpenSaveDialog(false);
			} else {
				SaveScene(scene);
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
			if (ImGui::GetIO().KeyShift) {
				session_.Redo(scene, fileStatus_);
			}
			else {
				session_.Undo(scene, fileStatus_);
			}
		}

		if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
			session_.Redo(scene, fileStatus_);
		}

		if (ImGui::IsKeyPressed(ImGuiKey_D, false)) {
			selection_.Duplicate(scene, fileStatus_);
		}
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
		selection_.Remove(scene, fileStatus_);
	}

#else
	(void)scene;

#endif
}

// 再生開始時に編集状態を退避し、停止時にはその状態へ戻す。
void Editor::TogglePlay (IEditableWorld& scene) {
	// 編集対象を退避して再生するか、元の配置へ復元して停止する。
	IEditableWorld& editWorld = GetEditWorld(scene);
	sceneView_.ResetInteraction ();

	std::string error;

	bool hasSucceeded = false;

	if (session_.IsPlaying ()) {
		hasSucceeded = session_.Stop (editWorld, error);

		if (hasSucceeded) {
			fileStatus_ = "Stopped. Edit-mode objects restored.";

			if (playOrigin_ == &scene) {
				playOrigin_ = nullptr;
			}

			selection_.Clear ();
		}
	} else {
		hasSucceeded = session_.BeginPlay (scene, error);

		if (hasSucceeded) {
			playOrigin_ = &scene;

			fileStatus_ = "Playing. Object edits will be discarded on Stop.";
		}
	}

	if (!hasSucceeded) {
		fileStatus_ = error;
	}
}

// 再生中の場面が変わっても、編集前のスナップショットは元の場面にだけ復元する。
IEditableWorld& Editor::GetEditWorld(IEditableWorld& scene) const {
	// 再生開始時の編集対象を取得する。
	if (playOrigin_ != nullptr) {
		return *playOrigin_;
	}

	return scene;
}
