#include "FrameProfiler.h"
#include "Editor.h"
#include "panels/hierarchy/HierarchyPanel.h"
#include "panels/inspector/InspectorPanel.h"
#include "serialization/SceneFile.h"
#include "LogSystem.h"
#include <utility>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

#ifdef USE_IMGUI

//=============================================================================================//
// 実装内補助定義

namespace {
	enum class PlaybackIcon { Play, Stop, Pause, Step };

	// フォントの収録文字や画像ファイルに依存しない図形アイコンを描く。
	bool DrawPlaybackButton (const char* identifier, PlaybackIcon icon, const char* hint, bool active) {
		if (active) {
			ImGui::PushStyleColor (ImGuiCol_Button, { 0.045f,0.10f,0.17f,1 });
		}

		const bool clicked = ImGui::Button (identifier, { 30,22 });

		if (active) {
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

		return clicked;
	}
}

#endif

//=============================================================================================//
// 初期化処理


void Editor::Initialize(IEditableWorld* scene, const std::string& initialPath) {
	scene_ = scene;

	playOrigin_ = nullptr;

	editParticleEditor_.reset();

	HierarchyPanel::Initialize();

	InspectorPanel::Initialize();

	sceneView_.Initialize();

	selection_.Clear();

	fileStatus_.clear();

	document_.Initialize("resources", AssetPathChange::Path(initialPath), fileStatus_);

	sceneFileDialog_ = SceneAssetDialog{};

	fileDialogAction_ = FileDialogAction::None;

	pendingScenePath_.clear();

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

// 再生の履歴は元の編集シーンに属する。遷移先の一時データで初期化し直さない。
void Editor::OnSceneChanged(IEditableWorld* scene, const char* sceneName, const std::string& initialPath) {
	if (playOrigin_ == nullptr) {
		Initialize(scene, initialPath);
	} else {
		if (scene != playOrigin_) {
			if (scene_ == playOrigin_) {
				// 元のプリセットの未保存状態を、遷移先の初期値で上書きしない。
				editParticleEditor_.emplace(std::move(particleEditor_));
			}

			particleEditor_ = ParticleEditorPanel{};

			if (scene != nullptr) {
				particleEditor_.Initialize(*scene);
			}
		} else {
			if (editParticleEditor_.has_value()) {
				particleEditor_ = std::move(*editParticleEditor_);

				editParticleEditor_.reset();
			}
		}

		scene_ = scene;

		selection_.Clear();

		sceneView_.ResetInteraction();

		HierarchyPanel::Initialize();

		InspectorPanel::Initialize();

		if (!session_.IsPlaying()) {
			playOrigin_ = nullptr;
		}
	}

	// モデルのサムネイルは旧シーンが所有するため、借用ハンドルを引き継がない。
	projectPanel_ = ProjectPanel{};

	sceneName_ = sceneName;
}

//=============================================================================================//
// 更新処理


void Editor::Update(IEditableWorld* scene) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::History);

	if (playOrigin_ != nullptr) {
		if (!session_.IsPlaying()) {
			// Stop直後はまだ遷移先を表示している。復帰前の一時配置を履歴へ入れない。
			return;
		}
	}

	if (scene == nullptr) {
		return;
	}

	if (!scene->SupportsObjectCollection()) {
		return;
	}

#ifdef USE_IMGUI

	// UI操作を確定してからショートカットを処理し、その結果も同じフレームで履歴へ記録する。
	session_.Update(GetEditWorld(*scene), ImGui::IsAnyItemActive() || sceneView_.IsManipulating());

	HandleShortcuts(*scene);

	session_.Update(GetEditWorld(*scene), ImGui::IsAnyItemActive() || sceneView_.IsManipulating());

#endif
	if (!fileStatus_.empty()) {
		if (fileStatus_ != loggedStatus_) {
			LogSystem::Log(fileStatus_ + "\n");

			loggedStatus_ = fileStatus_;
		}
	}
}

//=============================================================================================//
// 描画処理


void Editor::Draw(IEditableWorld* scene) {
	if (scene_ != scene) {
		Initialize(scene);
	}

	bool sceneAvailable = false;
	bool dirty = false;

	if (scene != nullptr) {
		if (scene->SupportsObjectCollection()) {
			sceneAvailable = true;

			dirty = NeedsSaveConfirmation(SceneAction::NewScene, *scene);
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

		performancePanel_.Draw(toolVisibility_.performance);

		return;
	}

	if (menuAction == EditorMenuAction::NewScene) {
		RequestSceneAction(SceneAction::NewScene, *scene);
	}

	if (menuAction == EditorMenuAction::SaveScene) {
		SaveScene(*scene);
	}

	if (menuAction == EditorMenuAction::SaveSceneAs) {
		OpenSaveDialog(false);
	}

	if (menuAction == EditorMenuAction::LoadScene) {
		OpenSceneDialog();
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

	projectPanel_.Draw(*scene, selection_, toolVisibility_, session_, particleEditor_, document_.GetPath());

	AssetPathChange pathChange;

	if (projectPanel_.ConsumeAssetPathChange(pathChange)) {
		document_.RemapAssetPaths(pathChange);
	}

	std::filesystem::path requestedScene;

	if (projectPanel_.ConsumeOpenScene(requestedScene)) {
		RequestSceneAction(SceneAction::LoadScene, *scene, requestedScene);
	}

	consolePanel_.Draw();

	sceneView_.Draw(*scene, selection_);

	InspectorPanel::DrawSelection(*scene,selection_,&toolVisibility_.particleEditor);

	scene->DrawEditorTools(toolVisibility_);

	performancePanel_.Draw(toolVisibility_.performance);

	DrawDiscardPrompt(*scene);

	DrawSceneAssetDialog(*scene);
}

//=============================================================================================//
// Getter / 再生・終了状態


// 一時停止とステップ要求を考慮し、このフレームのゲーム更新可否を返す。
bool Editor::ShouldSimulate () {
	return session_.ShouldSimulate ();
}

//=============================================================================================//
// 終了要求


// ウィンドウの終了要求を受け取り、必要なら未保存データの確認へ回す。
void Editor::RequestExit (IEditableWorld* scene) {
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

//=============================================================================================//
// シーン保存・切替


// 保存前に再生を終了し、編集用データだけをファイルへ書き込む。
bool Editor::SaveScene (IEditableWorld& scene) {
	std::string error;

	if (session_.IsPlaying ()) {
		if (!session_.Stop (GetEditWorld(scene), error)) {
			fileStatus_ = error;

			return false;
		}

		if (playOrigin_ == &scene) {
			playOrigin_ = nullptr;
		}
	}

	if (!document_.HasPath()) {
		OpenSaveDialog(pendingAction_ != SceneAction::None);

		return false;
	}

	if (!document_.Save(GetEditWorld(scene), error)) {
		fileStatus_ = error;

		return false;
	}

	session_.MarkSaved (GetEditWorld(scene));

	fileStatus_ = "Scene saved: " + AssetPathChange::Text(document_.GetPath());

	projectPanel_.RequestRefresh();

	return true;
}

void Editor::OpenSceneDialog() {
	if (pendingAction_ != SceneAction::None) {
		return;
	}

	fileDialogAction_ = FileDialogAction::Open;

	sceneFileDialog_.OpenForLoad();
}

void Editor::OpenSaveDialog(bool beforeAction, const std::filesystem::path& suggestion) {
	fileDialogAction_ = FileDialogAction::SaveAs;

	if (beforeAction) {
		fileDialogAction_ = FileDialogAction::SaveBeforeAction;
	}

	std::filesystem::path filename = suggestion;

	if (filename.empty()) {
		filename = "scenes/Untitled.ponscene";

		if (document_.HasPath()) {
			filename = document_.GetPath().lexically_relative(document_.GetRoot());
		}
	}

	sceneFileDialog_.OpenForSave(filename);
}

// 未保存の変更がある場合は、処理を保留して保存確認を表示する。
void Editor::RequestSceneAction(SceneAction action, IEditableWorld& scene, const std::filesystem::path& path) {
	if (pendingAction_ != SceneAction::None) {
		return;
	}

	if (action != SceneAction::Exit) {
		if (session_.IsPlaying()) {
			fileStatus_ = "Stop play mode before opening another scene.";

			return;
		}
	}

	pendingScenePath_ = path;

	pendingAction_ = action;

	if (NeedsSaveConfirmation(action, scene)) {
		openDiscardPrompt_ = true;
	} else {
		PerformSceneAction(scene);
	}
}

// 保存確認が済んだ新規作成・読込・終了を実行し、結果を画面へ通知する。
void Editor::PerformSceneAction(IEditableWorld& scene) {
	std::string error;

	bool succeeded = false;

	if (pendingAction_ == SceneAction::LoadScene) {
		succeeded = document_.Open(scene, pendingScenePath_, error);

		if (succeeded) {
			fileStatus_ = "Scene opened: " + AssetPathChange::Text(document_.GetPath());
		}
	} else if (pendingAction_ == SceneAction::NewScene) {
		succeeded = document_.New(scene, error);

		if (succeeded) {
			fileStatus_ = "New untitled scene. Choose a filename when saving.";
		}
	} else if (pendingAction_ == SceneAction::Exit) {
		exitApproved_ = true;

		succeeded = true;
	}

	if (succeeded) {
		if (pendingAction_ != SceneAction::Exit) {
			ResetSceneEditingState(scene);
		}
	} else {
		fileStatus_ = error;
	}

	pendingAction_ = SceneAction::None;

	pendingScenePath_.clear();
}

// 読込・新規作成の成功時だけ、前の配置に属する操作状態を解除する。
void Editor::ResetSceneEditingState(IEditableWorld& scene) {
	session_.Initialize(scene);

	selection_.Clear();

	sceneView_.Initialize();

	sceneView_.ShowScene();

	scene.SetEditorGameView(false);

	particleEditor_.ResetScenePreview(scene);

	HierarchyPanel::Initialize();

	InspectorPanel::Initialize();

	if (document_.HasPath()) {
		projectPanel_.RevealAsset(document_.GetPath());
	} else {
		projectPanel_.RequestRefresh();
	}
}

// 操作の種類と各保存対象の変更を順に確認する。
bool Editor::NeedsSaveConfirmation(SceneAction action, IEditableWorld& scene) const {
	if (!document_.HasPath()) {
		if (GetEditWorld(scene).GetEditableObjectCount() > 0) {
			return true;
		}
	}
	if (session_.IsDirty(GetEditWorld(scene))) {
		return true;
	}

	if (action == SceneAction::Exit) {
		if (IsParticlePresetDirty(scene)) {
			return true;
		}
	}

	return false;
}

bool Editor::IsParticlePresetDirty(IEditableWorld& scene) const {
	if (editParticleEditor_.has_value()) {
		return editParticleEditor_->IsDirty(GetEditWorld(scene));
	}

	return particleEditor_.IsDirty(GetEditWorld(scene));
}

bool Editor::SaveParticlePreset(IEditableWorld& scene) {
	if (editParticleEditor_.has_value()) {
		return editParticleEditor_->Save(GetEditWorld(scene));
	}

	return particleEditor_.Save(GetEditWorld(scene));
}

//=============================================================================================//
// パネル描画


// 再生・停止・一時停止・ステップだけを中央へ並べる。
void Editor::DrawToolbar (IEditableWorld& scene) {
#ifdef USE_IMGUI

	// シーン管理はメニューへ分離し、中央には再生操作だけを置く。
	// この行だけ縦の項目間隔を詰め、下の作業領域を広く保つ。
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, {ImGui::GetStyle().ItemSpacing.x, 2});

	const float controlsWidth = 3 * 30.0f + 2 * ImGui::GetStyle ().ItemSpacing.x;
	const float availableWidth = ImGui::GetContentRegionAvail ().x;
	const float toolbarStart = ImGui::GetCursorPosX();

	const ImVec2 toolbarOrigin = ImGui::GetCursorScreenPos();

	ImGui::GetWindowDrawList()->AddRectFilled(toolbarOrigin,{toolbarOrigin.x+availableWidth,toolbarOrigin.y+24},ImGui::GetColorU32(ImGuiCol_MenuBarBg));

	ImGui::SetCursorPosX(toolbarStart+10);

	ImGui::AlignTextToFramePadding();

	std::string displayName = document_.GetName();

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
void Editor::DrawDiscardPrompt (IEditableWorld& scene) {
#ifdef USE_IMGUI
	if (openDiscardPrompt_) {
		ImGui::OpenPopup ("Unsaved Scene");

		openDiscardPrompt_ = false;
	}

	if (ImGui::BeginPopupModal ("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted ("未保存の変更を保存してから続行しますか？");

		if (pendingAction_ == SceneAction::Exit) {
			if (IsParticlePresetDirty(scene)) {
				ImGui::TextUnformatted("Particleプリセットの変更も保存対象です。");
			}
		}

		if (ImGui::Button ("Save and Continue")) {
			bool saved = SaveScene (scene);

			if (saved) {
				if (pendingAction_ == SceneAction::Exit) {
					saved = SaveParticlePreset(scene);

					if (!saved) {
						fileStatus_ = "Particleプリセットを保存できません。設定値または保存先を確認してください。";
					}
				}
			}

			if (!saved) {
				if (fileDialogAction_ == FileDialogAction::SaveBeforeAction) {
					ImGui::CloseCurrentPopup();
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

			pendingScenePath_.clear();

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

// ファイル選択は結果を返すだけにし、保存成功後にのみ保留中の切替を続行する。
void Editor::DrawSceneAssetDialog(IEditableWorld& scene) {
	sceneFileDialog_.Draw(document_);

	std::filesystem::path selected;

	bool overwriteConfirmed = false;

	if (sceneFileDialog_.ConsumeSelection(selected, overwriteConfirmed)) {
		const FileDialogAction action = fileDialogAction_;

		fileDialogAction_ = FileDialogAction::None;

		if (action == FileDialogAction::Open) {
			RequestSceneAction(SceneAction::LoadScene, scene, selected);
		} else {
			if (document_.SaveAs(GetEditWorld(scene), selected, overwriteConfirmed, fileStatus_)) {
				session_.MarkSaved(GetEditWorld(scene));

				projectPanel_.RevealAsset(document_.GetPath());

				fileStatus_ = "Scene saved: " + AssetPathChange::Text(document_.GetPath());

				if (action == FileDialogAction::SaveBeforeAction) {
					bool canContinue = true;

					if (pendingAction_ == SceneAction::Exit) {
						canContinue = SaveParticlePreset(scene);
					}

					if (canContinue) {
						PerformSceneAction(scene);
					} else {
						fileStatus_ = "Particle preset save failed. The scene file was saved.";

						openDiscardPrompt_ = true;
					}
				}
			} else {
				OpenSaveDialog(action == FileDialogAction::SaveBeforeAction, selected.lexically_relative(document_.GetRoot()));

				sceneFileDialog_.SetStatus(fileStatus_);
			}
		}
	}

	if (sceneFileDialog_.ConsumeCancelled()) {
		if (fileDialogAction_ == FileDialogAction::SaveBeforeAction) {
			openDiscardPrompt_ = true;
		}

		fileDialogAction_ = FileDialogAction::None;
	}
}

//=============================================================================================//
// 入力・再生


// 文字入力やポップアップの操作と競合しないときだけショートカットを処理する。
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
	sceneView_.ResetInteraction ();

	std::string error;

	bool succeeded = false;

	if (session_.IsPlaying ()) {
		succeeded = session_.Stop (GetEditWorld(scene), error);

		if (succeeded) {
			fileStatus_ = "Stopped. Edit-mode objects restored.";

			if (playOrigin_ == &scene) {
				playOrigin_ = nullptr;
			}

			selection_.Clear ();
		}
	} else {
		succeeded = session_.BeginPlay (scene, error);

		if (succeeded) {
			playOrigin_ = &scene;

			fileStatus_ = "Playing. Object edits will be discarded on Stop.";
		}
	}

	if (!succeeded) {
		fileStatus_ = error;
	}
}

// 再生中の場面が変わっても、編集前のスナップショットは元の場面にだけ復元する。
IEditableWorld& Editor::GetEditWorld(IEditableWorld& scene) const {
	if (playOrigin_ != nullptr) {
		return *playOrigin_;
	}

	return scene;
}
