#include "FrameProfiler.h"
#include "Editor.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "SceneFile.h"
#include "LogSystem.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif


#ifdef USE_IMGUI
namespace {
	enum class PlaybackIcon { Play, Stop, Pause, Step };

	// フォントの収録文字や画像ファイルに依存しない図形アイコンを描く。
	bool DrawPlaybackButton (const char* identifier, PlaybackIcon icon, const char* hint, bool active) {
		if (active) {
			ImGui::PushStyleColor (ImGuiCol_Button, { 0.15f,0.46f,0.31f,1 });
		}
		const bool clicked = ImGui::Button (identifier, { 38,24 });
		if (active) {
			ImGui::PopStyleColor ();
		}
		const ImVec2 minimum = ImGui::GetItemRectMin ();
		const ImVec2 center = { minimum.x + 19, minimum.y + 12 };
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
		return clicked;
	}
}
#endif

// 保存前に再生を終了し、編集用データだけをファイルへ書き込む。
bool Editor::SaveScene (IEditableScene& scene) {
	std::string error;
	if (session_.IsPlaying ()) {
		if (!session_.Stop (scene, error)) {
			fileStatus_ = error;
			return false;
		}
	}
	if (!SceneFile::Save (scene, SceneFile::kDefaultPath, error)) {
		fileStatus_ = error;
		return false;
	}
	session_.MarkSaved (scene);
	fileStatus_ = "Scene saved.";
	return true;
}

// 操作の種類と各保存対象の変更を順に確認する。
bool Editor::NeedsSaveConfirmation(SceneAction action, IEditableScene& scene) const {
	if (session_.IsDirty(scene)) {
		return true;
	}
	if (action == SceneAction::Exit) {
		if (particleEditor_.IsDirty(scene)) {
			return true;
		}
	}
	return false;
}

// 未保存の変更がある場合は、処理を保留して保存確認を表示する。
void Editor::RequestSceneAction(SceneAction action, IEditableScene& scene) {
	pendingAction_ = action;
	if (NeedsSaveConfirmation(action, scene)) {
		openDiscardPrompt_ = true;
	} else {
		PerformSceneAction(scene);
	}
}

// 保存確認が済んだ新規作成・読込・終了を実行し、結果を画面へ通知する。
void Editor::PerformSceneAction (IEditableScene& scene) {
	std::string error;
	bool succeeded = false;
	if (pendingAction_ == SceneAction::LoadScene) {
		succeeded = SceneFile::Load (scene, SceneFile::kDefaultPath, error);
		if (succeeded) {
			session_.Initialize(scene);
			fileStatus_ = "Scene loaded.";
		}
	} else if (pendingAction_ == SceneAction::NewScene) {
		succeeded = scene.ReplaceObjects ({}, error);
		if (succeeded) {
			session_.Update(scene, false);
			fileStatus_ = "New empty scene. Save Scene to keep it.";
		}
	} else if (pendingAction_ == SceneAction::Exit) {
		exitApproved_ = true;
		succeeded = true;
	}
	if (succeeded) {
		selection_.Clear ();
		sceneView_.ResetInteraction ();
	} else {
		fileStatus_ = error;
	}
	pendingAction_ = SceneAction::None;
}

// 再生開始時に編集状態を退避し、停止時にはその状態へ戻す。
void Editor::TogglePlay (IEditableScene& scene) {
	sceneView_.ResetInteraction ();
	std::string error;
	bool succeeded = false;
	if (session_.IsPlaying ()) {
		succeeded = session_.Stop (scene, error);
		if (succeeded) {
			fileStatus_ = "Stopped. Edit-mode objects restored.";
			selection_.Clear ();
		}
	} else {
		succeeded = session_.BeginPlay (scene, error);
		if (succeeded) {
			fileStatus_ = "Playing. Object edits will be discarded on Stop.";
		}
	}
	if (!succeeded) {
		fileStatus_ = error;
	}
}

// 一時停止とステップ要求を考慮し、このフレームのゲーム更新可否を返す。
bool Editor::ShouldSimulate () {
	return session_.ShouldSimulate ();
}

// ウィンドウの終了要求を受け取り、必要なら未保存データの確認へ回す。
void Editor::RequestExit (IEditableScene* scene) {
	if (pendingAction_ != SceneAction::None) {
		return;
	}
	if (scene == nullptr) {
		exitApproved_ = true;
		return;
	}
	if (!scene->SupportsObjectCollection()) {
		exitApproved_ = true;
		return;
	}
	RequestSceneAction (SceneAction::Exit, *scene);
}

// 再生・停止・一時停止・ステップだけを中央へ並べる。
void Editor::DrawToolbar (IEditableScene& scene) {
#ifdef USE_IMGUI
	// シーン管理はメニューへ分離し、中央には再生操作だけを置く。
	// この行だけ縦の項目間隔を詰め、下の作業領域を広く保つ。
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, 2});
	const float controlsWidth = 3 * 38.0f + 2 * ImGui::GetStyle ().ItemSpacing.x;
	const float availableWidth = ImGui::GetContentRegionAvail ().x;
	if (availableWidth > controlsWidth) {
		ImGui::SetCursorPosX (ImGui::GetCursorPosX () + (availableWidth - controlsWidth) * 0.5f);
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
	ImGui::BeginDisabled (!session_.IsPlaying () || !session_.IsPaused ());
	if (DrawPlaybackButton ("##Step", PlaybackIcon::Step, "1フレーム進める", false)) {
		session_.RequestStep ();
	}
	ImGui::EndDisabled ();
	ImGui::PopStyleVar();

#else
	(void) scene;
#endif
}
// 保存成功または破棄の選択が済むまで、シーンを置き換えない。
void Editor::DrawDiscardPrompt (IEditableScene& scene) {
#ifdef USE_IMGUI
	if (openDiscardPrompt_) {
		ImGui::OpenPopup ("Unsaved Scene");
		openDiscardPrompt_ = false;
	}
	if (ImGui::BeginPopupModal ("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted ("未保存の変更を保存してから続行しますか？");
		if (pendingAction_ == SceneAction::Exit) {
			if (particleEditor_.IsDirty(scene)) {
				ImGui::TextUnformatted("Particleプリセットの変更も保存対象です。");
			}
		}
		if (ImGui::Button ("Save and Continue")) {
			bool saved = SaveScene (scene);
			if (saved) {
				if (pendingAction_ == SceneAction::Exit) {
					saved = particleEditor_.Save(scene);
					if (!saved) {
						fileStatus_ = "Particleプリセットを保存できません。設定値または保存先を確認してください。";
					}
				}
			}
			if (saved) {
				PerformSceneAction (scene);
				ImGui::CloseCurrentPopup ();
			}
		}
		ImGui::SameLine ();
		if (ImGui::Button ("Discard and Continue")) {
			PerformSceneAction (scene);
			ImGui::CloseCurrentPopup ();
		}
		ImGui::SameLine ();
		if (ImGui::Button ("Cancel")) {
			pendingAction_ = SceneAction::None;
			ImGui::CloseCurrentPopup ();
		}
		if (!fileStatus_.empty ()) {
			ImGui::TextWrapped ("%s", fileStatus_.c_str ());
		}
		ImGui::EndPopup ();
	}
#else
	(void) scene;
#endif
}

// 文字入力やポップアップの操作と競合しないときだけショートカットを処理する。
void Editor::HandleShortcuts(IEditableScene& scene) {
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
			RequestSceneAction(SceneAction::LoadScene, scene);
		}
		if (ImGui::IsKeyPressed(ImGuiKey_S, false)) {
			SaveScene(scene);
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

//=============================================================================================//
// 初期化処理

void Editor::Initialize(IEditableScene* scene) {
	scene_ = scene;
	HierarchyPanel::Initialize();
	InspectorPanel::Initialize();
	sceneView_.Initialize();
	selection_.Clear();
	fileStatus_.clear();
	loggedStatus_.clear();
	pendingAction_ = SceneAction::None;
	openDiscardPrompt_ = false;
	exitApproved_ = false;
	if (scene != nullptr) {
		session_.Initialize(*scene);
		particleEditor_.Initialize(*scene);
	} else {
		session_ = EditorSession{};
	}
}

//=============================================================================================//
// 更新処理

void Editor::Update(IEditableScene* scene) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::History);
	if (scene == nullptr) {
		return;
	}
	if (!scene->SupportsObjectCollection()) {
		return;
	}
#ifdef USE_IMGUI
	// UI操作を確定してからショートカットを処理し、その結果も同じフレームで履歴へ記録する。
	session_.Update(*scene, ImGui::IsAnyItemActive() || sceneView_.IsManipulating());
	HandleShortcuts(*scene);
	session_.Update(*scene, ImGui::IsAnyItemActive() || sceneView_.IsManipulating());
#endif
	if (!fileStatus_.empty()) {
		if (fileStatus_ != loggedStatus_) {
			LogSystem::Log(fileStatus_ + "\n");
			loggedStatus_ = fileStatus_;
		}
	}
}

//=============================================================================================//
// メンバ関数 / Inspector描画

void Editor::DrawInspector(IEditableScene& scene) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Inspector);
	selection_.Prune(scene);
	if (selection_.GetIds().size() > 1) {
		DrawMultipleInspector(scene);
		return;
	}
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == kInvalidSceneObjectId) {
			continue;
		}
		if (object.id == selection_.Get()) {
			InspectorPanel::Draw(object, &toolVisibility_.particleEditor, &scene);
			return;
		}
	}
	selection_.Clear();
	InspectorPanel::Draw({});
}

//=============================================================================================//
// 描画処理

void Editor::Draw(IEditableScene* scene) {
	if (scene_ != scene) {
		Initialize(scene);
	}
	bool sceneAvailable = false;
	bool dirty = false;
	if (scene != nullptr) {
		if (scene->SupportsObjectCollection()) {
			sceneAvailable = true;
			dirty = session_.IsDirty(*scene);
		}
	}
	const EditorMenuAction menuAction = layout_.Begin(sceneAvailable, session_.IsPlaying(), dirty, toolVisibility_);
	if (sceneAvailable) {
		DrawToolbar(*scene);
	}
	layout_.Draw(fileStatus_);
	if (!sceneAvailable) {
		HierarchyPanel::Draw(scene, selection_);
		InspectorPanel::Draw({});
		DrawPerformance();
		return;
	}
	if (menuAction == EditorMenuAction::NewScene) {
		RequestSceneAction(SceneAction::NewScene, *scene);
	}
	if (menuAction == EditorMenuAction::SaveScene) {
		SaveScene(*scene);
	}
	if (menuAction == EditorMenuAction::LoadScene) {
		RequestSceneAction(SceneAction::LoadScene, *scene);
	}
	HierarchyPanel::Draw(scene, selection_);
	if (toolVisibility_.particleEditor) {
		particleEditor_.Draw(*scene, session_.IsPlaying(), selection_.Get(), &toolVisibility_.particleEditor);
	} else {
		particleEditor_.StopPreview(*scene, session_.IsPlaying());
	}
	if (particleEditor_.ConsumeFocusRequest()) {
		sceneView_.ShowScene();
		scene->FocusEditorCamera(particleEditor_.GetFocusPosition());
	}
	utilityPanels_.Draw(*scene, selection_, toolVisibility_);
	sceneView_.Draw(*scene, selection_);
	DrawInspector(*scene);
	scene->DrawEditorTools(toolVisibility_);
	DrawPerformance();
	DrawDiscardPrompt(*scene);
}
