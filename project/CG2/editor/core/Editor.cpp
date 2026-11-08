#include "FrameProfiler.h"
#include "Editor.h"
#include "../panels/hierarchy/HierarchyPanel.h"
#include "../panels/inspector/InspectorPanel.h"
#include "../ui/EditorTheme.h"
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

//=============================================================================================//
// 初期化処理


void Editor::InitializeUi() {
#ifdef USE_IMGUI
	// 最初のフレームでEditor用の配置を読み込めるよう、保存先と操作方法を先に設定する。
	ImGuiIO& inputOutput = ImGui::GetIO();

	inputOutput.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	inputOutput.IniFilename = "editor-layout.ini";
	inputOutput.ConfigWindowsMoveFromTitleBarOnly = true;

	// 配色、余白、フォント倍率をEditorのテーマへそろえる。
	EditorTheme::Initialize();
#endif
}

void Editor::Initialize(IEditableWorld* scene, const std::string& initialPath) {
	// 編集対象を切り替え、選択・履歴・保存確認を初期状態へ戻す。
	scene_ = scene;

	playOrigin_ = nullptr;

	editParticleEditor_.reset();

	ResetPanelSelection();

	sceneView_.Initialize();

	fileStatus_.clear();

	// 起動時の保存先をScene管理へ渡す。
	document_.Initialize("resources", AssetPathChange::Path(initialPath), fileStatus_);

	sceneFileDialog_ = SceneAssetDialog{};

	fileDialogAction_ = FileDialogAction::None;

	pendingScenePath_.clear();

	loggedStatus_.clear();

	pendingAction_ = SceneAction::None;

	shouldOpenDiscardPrompt_ = false;

	isExitApproved_ = false;

	if (scene != nullptr) {
		// 現在の配置を履歴と保存済み状態の基準にする。
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

		sceneView_.ResetInteraction();

		ResetPanelSelection();

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

	if (!scene->HasObjectCollectionSupport()) {
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
	// メニューの操作を受け取り、選択状態を共有して各パネルを描く。
	if (scene_ != scene) {
		Initialize(scene);
	}

	bool isSceneAvailable = false;
	bool hasUnsavedChanges = false;

	if (scene != nullptr) {
		if (scene->HasObjectCollectionSupport()) {
			isSceneAvailable = true;

			hasUnsavedChanges = IsSaveConfirmationNeeded(SceneAction::NewScene, *scene);
		}
	}

	// メニューの操作を受け取り、各パネルより先に処理する。
	const EditorMenuAction menuAction = layout_.Begin(isSceneAvailable, session_.IsPlaying(), hasUnsavedChanges, toolVisibility_);

	if (isSceneAvailable) {
		DrawToolbar(*scene);
	}

	layout_.Draw(fileStatus_);

	if (!isSceneAvailable) {
		// 編集可能なシーンがなくても、空のパネルを表示する。
		HierarchyPanel::Draw(scene, selection_);

		InspectorPanel::Draw({});

		performancePanel_.Draw(toolVisibility_.isPerformanceVisible);

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

	if (toolVisibility_.isParticleEditorVisible) {
		particleEditor_.Draw(*scene, session_.IsPlaying(), selection_.GetPrimarySelectedObjectId(), &toolVisibility_.isParticleEditorVisible);
	} else {
		particleEditor_.StopPreview(*scene, session_.IsPlaying());
	}

	if (particleEditor_.ConsumeFocusRequest()) {
		sceneView_.ShowScene();

		scene->FocusEditorCamera(particleEditor_.GetFocusPosition());
	}

	// ProjectからのScene読込とAsset移動の結果を受け取る。
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
	InspectorPanel::DrawSelection(*scene,selection_,&toolVisibility_.isParticleEditorVisible);
	scene->DrawEditorTools(toolVisibility_);
	performancePanel_.Draw(toolVisibility_.isPerformanceVisible);

	// 配置を失う操作は、保存確認を通してから実行する。
	DrawDiscardPrompt(*scene);
	DrawSceneAssetDialog(*scene);
}

//=============================================================================================//
// Getter / 再生・終了状態

bool Editor::IsPlaying() const {
	// 編集モードとゲームの再生状態を区別する。
	if (session_.IsPlaying()) {
		return true;
	}

	return false;
}

// 一時停止とステップ要求を考慮し、このフレームのゲーム更新可否を返す。
bool Editor::ShouldSimulate () {
	// ステップ要求を一度だけ取り出し、ゲーム更新の可否を返す。
	if (session_.ShouldSimulate()) {
		return true;
	}

	return false;
}

bool Editor::ShouldExit() const {
	// 未保存の確認を経て、終了が許可されているか返す。
	if (isExitApproved_) {
		return true;
	}

	return false;
}

const std::filesystem::path& Editor::GetScenePath() const {
	// 編集中のSceneファイルの保存先を取得する。
	return document_.GetPath();
}

//=============================================================================================//
// Getter / Scene表示サイズ

uint32_t Editor::GetSceneWidth() const {
	// SceneViewの描画先の幅を取得する。
	return sceneView_.GetWidth();
}

uint32_t Editor::GetSceneHeight() const {
	// SceneViewの描画先の高さを取得する。
	return sceneView_.GetHeight();
}

//=============================================================================================//
// Setter / Scene画像

// 描画先が所有するSRVハンドルを借用する。
void Editor::SetSceneTexture(uint64_t textureHandle) {
	// 描画側が所有するScene画像のハンドルを借用する。
	sceneView_.SetTexture(textureHandle);
}

//=============================================================================================//
// 終了要求


// ウィンドウの終了要求を受け取り、必要なら未保存データの確認へ回す。
void Editor::RequestExit (IEditableWorld* scene) {
	// 未保存の変更を確認し、終了可能になるまで要求を保留する。
	if (pendingAction_ != SceneAction::None) {
		return;
	}

	if (scene == nullptr) {
		isExitApproved_ = true;

		return;
	}

	if (!scene->HasObjectCollectionSupport()) {
		isExitApproved_ = true;

		return;
	}

	RequestSceneAction (SceneAction::Exit, *scene);
}

//=============================================================================================//
// シーン保存・切替


// 保存前に再生を終了し、編集用データだけをファイルへ書き込む。
bool Editor::SaveScene (IEditableWorld& scene) {
	// 再生中なら編集用の配置へ戻し、現在の保存先へ書き込む。
	std::string error;

	// 保存する配置を、再生前の編集状態へ戻す。
	if (session_.IsPlaying ()) {
		if (!session_.Stop (GetEditWorld(scene), error)) {
			fileStatus_ = error;

			return false;
		}

		if (playOrigin_ == &scene) {
			playOrigin_ = nullptr;
		}
	}

	// 未保存のシーンでは先に保存先を選ぶ。
	if (!document_.HasPath()) {
		OpenSaveDialog(pendingAction_ != SceneAction::None);

		return false;
	}

	if (!document_.Save(GetEditWorld(scene), error)) {
		fileStatus_ = error;

		return false;
	}

	// 書込成功後にだけ、未保存判定の基準を更新する。
	session_.MarkSaved (GetEditWorld(scene));

	fileStatus_ = "Scene saved: " + AssetPathChange::Text(document_.GetPath());

	projectPanel_.RequestRefresh();

	return true;
}

void Editor::OpenSceneDialog() {
	// 別の操作の確認中でなければ、Sceneの選択画面を予約する。
	if (pendingAction_ != SceneAction::None) {
		return;
	}

	fileDialogAction_ = FileDialogAction::Open;

	sceneFileDialog_.OpenForLoad();
}

void Editor::OpenSaveDialog(bool shouldSaveBeforeAction, const std::filesystem::path& suggestion) {
	// 現在の保存先を候補にし、必要なら未保存確認後の操作も保留する。
	fileDialogAction_ = FileDialogAction::SaveAs;

	if (shouldSaveBeforeAction) {
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
	// 開く対象を保持し、必要な保存確認が済むまで切替を待つ。
	if (pendingAction_ != SceneAction::None) {
		return;
	}

	if (action != SceneAction::Exit) {
		if (session_.IsPlaying()) {
			fileStatus_ = "Stop play mode before opening another scene.";

			return;
		}
	}

	// 保存確認で待つ場合も、利用者が選んだファイルを保持する。
	pendingScenePath_ = path;

	pendingAction_ = action;

	if (IsSaveConfirmationNeeded(action, scene)) {
		// 未保存の編集があるときは、読み込みを保留して確認を表示する。
		shouldOpenDiscardPrompt_ = true;
	} else {
		PerformSceneAction(scene);
	}
}

// 保存確認が済んだ新規作成・読込・終了を実行し、結果を画面へ通知する。
void Editor::PerformSceneAction(IEditableWorld& scene) {
	// 確認済みの新規作成・読込・終了を実行し、保留状態を解除する。
	std::string error;

	bool hasSucceeded = false;

	if (pendingAction_ == SceneAction::LoadScene) {
		// 保留していたパスを SceneDocument に渡し、検証と読み込みを実行する。
		hasSucceeded = document_.Open(scene, pendingScenePath_, error);

		// 読込に成功した保存先を通知する。
		if (hasSucceeded) {
			fileStatus_ = "Scene opened: " + AssetPathChange::Text(document_.GetPath());
		}
	} else if (pendingAction_ == SceneAction::NewScene) {
		hasSucceeded = document_.New(scene, error);

		if (hasSucceeded) {
			fileStatus_ = "New untitled scene. Choose a filename when saving.";
		}
	} else if (pendingAction_ == SceneAction::Exit) {
		isExitApproved_ = true;

		hasSucceeded = true;
	}

	if (hasSucceeded) {
		if (pendingAction_ != SceneAction::Exit) {
			// 読み込みや新規作成に成功したら、前の Scene の選択や編集履歴を解除する。
			ResetSceneEditingState(scene);
		}
	} else {
		// パス検証やファイル解析が返した失敗理由を、画面に表示する。
		fileStatus_ = error;
	}

	// 操作の成否にかかわらず、確認済みの要求を消す。
	pendingAction_ = SceneAction::None;

	pendingScenePath_.clear();
}

// 読込・新規作成の成功時だけ、前の配置に属する操作状態を解除する。
void Editor::ResetSceneEditingState(IEditableWorld& scene) {
	// シーン切替の成功後に、選択・履歴・各パネルの操作状態を解除する。
	session_.Initialize(scene);

	ResetPanelSelection();

	sceneView_.Initialize();

	sceneView_.ShowScene();

	scene.SetEditorGameView(false);

	// 共通プリセットの保存基準を残し、プレビューだけ止める。
	particleEditor_.ResetScenePreview(scene);

	if (document_.HasPath()) {
		projectPanel_.RevealAsset(document_.GetPath());
	} else {
		projectPanel_.RequestRefresh();
	}
}

void Editor::ResetPanelSelection() {
	// 前のシーンで選んだIDと、パネルに残っている操作対象・通知を消す。
	selection_.Clear();

	HierarchyPanel::Initialize();

	InspectorPanel::Initialize();
}

// 操作の種類と各保存対象の変更を順に確認する。
bool Editor::IsSaveConfirmationNeeded(SceneAction action, IEditableWorld& scene) const {
	// 保留中の操作で失われる未保存の設定があるか確認する。
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
	// 遷移前の編集対象も考慮して、共通Particle設定の変更を確認する。
	if (editParticleEditor_.has_value()) {
		return editParticleEditor_->IsDirty(GetEditWorld(scene));
	}

	return particleEditor_.IsDirty(GetEditWorld(scene));
}

bool Editor::SaveParticlePreset(IEditableWorld& scene) {
	// 再生前の編集対象を使い、共通Particleプリセットを保存する。
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
void Editor::DrawDiscardPrompt (IEditableWorld& scene) {
	// 保留中の操作に対して、保存・破棄・取消の選択を受け取る。
#ifdef USE_IMGUI
	if (shouldOpenDiscardPrompt_) {
		ImGui::OpenPopup ("Unsaved Scene");

		shouldOpenDiscardPrompt_ = false;
	}

	if (ImGui::BeginPopupModal ("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted ("未保存の変更を保存してから続行しますか？");

		if (pendingAction_ == SceneAction::Exit) {
			if (IsParticlePresetDirty(scene)) {
				ImGui::TextUnformatted("Particleプリセットの変更も保存対象です。");
			}
		}

		if (ImGui::Button ("Save and Continue")) {
			bool wasSaved = SaveScene (scene);

			if (wasSaved) {
				if (pendingAction_ == SceneAction::Exit) {
					wasSaved = SaveParticlePreset(scene);

					if (!wasSaved) {
						fileStatus_ = "Particleプリセットを保存できません。設定値または保存先を確認してください。";
					}
				}
			}

			if (!wasSaved) {
				if (fileDialogAction_ == FileDialogAction::SaveBeforeAction) {
					ImGui::CloseCurrentPopup();
				}
			}

			if (wasSaved) {
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
	// ファイル選択の結果を受け取り、成功時だけ保存先や配置を切り替える。
	sceneFileDialog_.Draw(document_);

	std::filesystem::path selected;

	bool isOverwriteConfirmed = false;

	// 選択結果を一度だけ取り出す。
	if (sceneFileDialog_.ConsumeSelection(selected, isOverwriteConfirmed)) {
		const FileDialogAction action = fileDialogAction_;

		fileDialogAction_ = FileDialogAction::None;

		if (action == FileDialogAction::Open) {
			// 選んだパスを読み込み要求として渡し、未保存の編集を確認する。
			RequestSceneAction(SceneAction::LoadScene, scene, selected);
		} else {
			if (document_.SaveAs(GetEditWorld(scene), selected, isOverwriteConfirmed, fileStatus_)) {
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

						shouldOpenDiscardPrompt_ = true;
					}
				}
			} else {
				OpenSaveDialog(action == FileDialogAction::SaveBeforeAction, selected.lexically_relative(document_.GetRoot()));

				sceneFileDialog_.SetStatus(fileStatus_);
			}
		}
	}

	// 保存先の選択を取り消したら、元の確認へ戻す。
	if (sceneFileDialog_.ConsumeCancelled()) {
		if (fileDialogAction_ == FileDialogAction::SaveBeforeAction) {
			shouldOpenDiscardPrompt_ = true;
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
	// 編集対象を退避して再生するか、元の配置へ復元して停止する。
	sceneView_.ResetInteraction ();

	std::string error;

	bool hasSucceeded = false;

	if (session_.IsPlaying ()) {
		hasSucceeded = session_.Stop (GetEditWorld(scene), error);

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
