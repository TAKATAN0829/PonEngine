#include "Editor.h"
#include "serialization/SceneFile.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// シーン保存・切替


// 保存前に再生を終了し、編集用データだけをファイルへ書き込む。
bool Editor::SaveScene (IEditableWorld& scene) {
	// 再生中なら編集用の配置へ戻し、現在の保存先へ書き込む。
	IEditableWorld& editWorld = GetEditWorld(scene);
	std::string error;

	// 保存する配置を、再生前の編集状態へ戻す。
	if (session_.IsPlaying ()) {
		if (!session_.Stop (editWorld, error)) {
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

	if (!document_.Save(editWorld, error)) {
		fileStatus_ = error;

		return false;
	}

	// 書込成功後にだけ、未保存判定の基準を更新する。
	session_.MarkSaved (editWorld);

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
		hasSucceeded = document_.Open(scene, scene, pendingScenePath_, error);

		// 読込に成功した保存先を通知する。
		if (hasSucceeded) {
			fileStatus_ = "Scene opened: " + AssetPathChange::Text(document_.GetPath());
		}
	} else if (pendingAction_ == SceneAction::NewScene) {
		hasSucceeded = document_.New(scene, scene, error);

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
	particleEditor_.ResetScenePreview(scene, scene);

	if (document_.HasPath()) {
		projectPanel_.RevealAsset(document_.GetPath());
	} else {
		projectPanel_.RequestRefresh();
	}
}

void Editor::ResetPanelSelection() {
	// 前のシーンで選んだIDと、パネルに残っている操作対象・通知を消す。
	selection_.Clear();

	hierarchyPanel_.Initialize();

	inspectorPanel_.Initialize();
}

// 操作の種類と各保存対象の変更を順に確認する。
bool Editor::IsSaveConfirmationNeeded(SceneAction action, IEditableWorld& scene) const {
	// 保留中の操作で失われる未保存の設定があるか確認する。
	IEditableWorld& editWorld = GetEditWorld(scene);

	if (!document_.HasPath()) {
		if (editWorld.GetEditableObjectCount() > 0) {
			return true;
		}
	}
	if (session_.IsDirty(editWorld)) {
		return true;
	}

	if (action == SceneAction::Exit) {
		bool isPresetDirty = false;

		if (editParticleEditor_.has_value()) {
			isPresetDirty = editParticleEditor_->IsDirty(editWorld, editWorld);
		} else {
			isPresetDirty = particleEditor_.IsDirty(editWorld, editWorld);
		}

		if (isPresetDirty) {
			return true;
		}
	}

	return false;
}

//=============================================================================================//
// パネル描画


// 再生・停止・一時停止・ステップだけを中央へ並べる。
void Editor::DrawDiscardPrompt (IEditableWorld& scene) {
	// 保留中の操作に対して、保存・破棄・取消の選択を受け取る。
#ifdef USE_IMGUI
	IEditableWorld& editWorld = GetEditWorld(scene);

	if (shouldOpenDiscardPrompt_) {
		ImGui::OpenPopup ("Unsaved Scene");

		shouldOpenDiscardPrompt_ = false;
	}

	if (ImGui::BeginPopupModal ("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		ImGui::TextUnformatted ("未保存の変更を保存してから続行しますか？");

		if (pendingAction_ == SceneAction::Exit) {
			bool isPresetDirty = false;

			if (editParticleEditor_.has_value()) {
				isPresetDirty = editParticleEditor_->IsDirty(editWorld, editWorld);
			} else {
				isPresetDirty = particleEditor_.IsDirty(editWorld, editWorld);
			}

			if (isPresetDirty) {
				ImGui::TextUnformatted("Particleプリセットの変更も保存対象です。");
			}
		}

		if (ImGui::Button ("Save and Continue")) {
			bool wasSaved = SaveScene (scene);

			if (wasSaved) {
				if (pendingAction_ == SceneAction::Exit) {
					if (editParticleEditor_.has_value()) {
						wasSaved = editParticleEditor_->Save(editWorld);
					} else {
						wasSaved = particleEditor_.Save(editWorld);
					}

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
			IEditableWorld& editWorld = GetEditWorld(scene);
			if (document_.SaveAs(editWorld, selected, isOverwriteConfirmed, fileStatus_)) {
				session_.MarkSaved(editWorld);

				projectPanel_.RevealAsset(document_.GetPath());

				fileStatus_ = "Scene saved: " + AssetPathChange::Text(document_.GetPath());

				if (action == FileDialogAction::SaveBeforeAction) {
					bool canContinue = true;

					if (pendingAction_ == SceneAction::Exit) {
						if (editParticleEditor_.has_value()) {
							canContinue = editParticleEditor_->Save(editWorld);
						} else {
							canContinue = particleEditor_.Save(editWorld);
						}
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
