#include "FrameProfiler.h"
#include "Editor.h"
#include "../panels/hierarchy/HierarchyPanel.h"
#include "../panels/inspector/InspectorPanel.h"
#include "../ui/EditorTheme.h"
#include "LogSystem.h"
#include <utility>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
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

		particleEditor_.Initialize(*scene, *scene);
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
				particleEditor_.Initialize(*scene, *scene);
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
	IEditableWorld& editWorld = GetEditWorld(*scene);
	const bool isInteractionActive = ImGui::IsAnyItemActive() || sceneView_.IsManipulating();
	session_.Update(editWorld, isInteractionActive);

	HandleShortcuts(*scene);

	session_.Update(editWorld, isInteractionActive);

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
		hierarchyPanel_.Draw(scene, selection_);

		inspectorPanel_.Draw({});

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

	// パネルには、それぞれが必要とするシーン機能だけを渡す。
	IWorldObjects& objects = *scene;
	IParticlePreviewWorld& particleWorld = *scene;
	IModelAssetAccess& models = *scene;
	ITextureAssetAccess& textures = *scene;
	IAssetReferenceRemapper& assetReferences = *scene;
	ISceneViewWorld& sceneViewWorld = *scene;
	IEditorToolsWorld& editorTools = *scene;

	hierarchyPanel_.Draw(&objects, selection_);

	if (toolVisibility_.isParticleEditorVisible) {
		particleEditor_.Draw(objects, particleWorld, textures, session_.IsPlaying(),
			selection_.GetPrimarySelectedObjectId(), &toolVisibility_.isParticleEditorVisible);
	} else {
		particleEditor_.StopPreview(particleWorld, session_.IsPlaying());
	}

	if (particleEditor_.ConsumeFocusRequest()) {
		sceneView_.ShowScene();

		sceneViewWorld.FocusEditorCamera(particleEditor_.GetFocusPosition());
	}

	// ProjectからのScene読込とAsset移動の結果を受け取る。
	projectPanel_.Draw(objects, particleWorld, models, assetReferences, selection_, toolVisibility_, session_,
		particleEditor_, document_.GetPath());

	AssetPathChange pathChange;

	if (projectPanel_.ConsumeAssetPathChange(pathChange)) {
		document_.RemapAssetPaths(pathChange);
	}

	std::filesystem::path requestedScene;

	if (projectPanel_.ConsumeOpenScene(requestedScene)) {
		RequestSceneAction(SceneAction::LoadScene, *scene, requestedScene);
	}

	consolePanel_.Draw();
	sceneView_.Draw(objects, sceneViewWorld, selection_);
	inspectorPanel_.DrawSelection(objects, models, textures, selection_, &toolVisibility_.isParticleEditorVisible);
	editorTools.DrawEditorTools(toolVisibility_);
	performancePanel_.Draw(toolVisibility_.isPerformanceVisible);

	// 配置を失う操作は、保存確認を通してから実行する。
	DrawDiscardPrompt(*scene);
	DrawSceneAssetDialog(*scene);
}

//=============================================================================================//
// Getter / 再生・終了状態

bool Editor::IsPlaying() const {
	// 編集モードとゲームの再生状態を区別する。
	return session_.IsPlaying();
}

// 一時停止とステップ要求を考慮し、このフレームのゲーム更新可否を返す。
bool Editor::ShouldSimulate () {
	// ステップ要求を一度だけ取り出し、ゲーム更新の可否を返す。
	return session_.ShouldSimulate();
}

bool Editor::ShouldExit() const {
	// 未保存の確認を経て、終了が許可されているか返す。
	return isExitApproved_;
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
