#include "Editor.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "SceneFile.h"
#include "LogSystem.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// 保存前に再生を終了し、編集用データだけをファイルへ書き込む。
bool Editor::SaveScene(IEditableScene& scene) {
    std::string error;
    if (session_.IsPlaying()) {
        if (!session_.Stop(scene, error)) {
            fileStatus_ = error;
            return false;
        }
    }
    if (!SceneFile::Save(scene, SceneFile::kDefaultPath, error)) {
        fileStatus_ = error;
        return false;
    }
    session_.MarkSaved(scene);
    fileStatus_ = "Scene saved.";
    return true;
}

// 未保存の変更がある場合は、処理を保留して保存確認を表示する。
void Editor::RequestSceneAction(SceneAction action, IEditableScene& scene) {
    pendingAction_ = action;
    if (session_.IsDirty(scene) || (action == SceneAction::Exit && particleEditor_.IsDirty(scene))) {
        openDiscardPrompt_ = true;
    } else {
        PerformSceneAction(scene);
    }
}

// 保存確認が済んだ新規作成・読込・終了を実行し、結果を画面へ通知する。
void Editor::PerformSceneAction(IEditableScene& scene) {
    std::string error;
    bool succeeded = false;
    if (pendingAction_ == SceneAction::LoadScene) {
        succeeded = SceneFile::Load(scene, SceneFile::kDefaultPath, error);
        if (succeeded) {
            session_.Reset(scene);
            fileStatus_ = "Scene loaded.";
        }
    } else if (pendingAction_ == SceneAction::NewScene) {
        succeeded = scene.ReplaceObjects({}, error);
        if (succeeded) {
            session_.RecordChanges(scene, false);
            fileStatus_ = "New empty scene. Save Scene to keep it.";
        }
    } else if (pendingAction_ == SceneAction::Exit) {
        exitApproved_ = true;
        succeeded = true;
    }
    if (succeeded) {
        selection_.Clear();
        sceneView_.ResetInteraction();
    } else {
        fileStatus_ = error;
    }
    pendingAction_ = SceneAction::None;
}

// 再生開始時に編集状態を退避し、停止時にはその状態へ戻す。
void Editor::TogglePlay(IEditableScene& scene) {
    sceneView_.ResetInteraction();
    std::string error;
    bool succeeded = false;
    if (session_.IsPlaying()) {
        succeeded = session_.Stop(scene, error);
        if (succeeded) {
            fileStatus_ = "Stopped. Edit-mode objects restored.";
            selection_.Clear();
        }
    } else {
        succeeded = session_.BeginPlay(scene, error);
        if (succeeded) {
            fileStatus_ = "Playing. Object edits will be discarded on Stop.";
        }
    }
    if (!succeeded) {
        fileStatus_ = error;
    }
}

// 一時停止とステップ要求を考慮し、このフレームのゲーム更新可否を返す。
bool Editor::ShouldSimulate() {
    return session_.ShouldSimulate();
}

// ウィンドウの終了要求を受け取り、必要なら未保存データの確認へ回す。
void Editor::RequestExit(IEditableScene* scene) {
    if (pendingAction_ != SceneAction::None) {
        return;
    }
    if (scene == nullptr || !scene->SupportsObjectCollection()) {
        exitApproved_ = true;
        return;
    }
    RequestSceneAction(SceneAction::Exit, *scene);
}

// 保存・履歴・再生操作をまとめ、現在のモードで使えない操作は無効にする。
void Editor::DrawToolbar(IEditableScene& scene) {
#ifdef USE_IMGUI
    if (ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
        ImGui::TextDisabled("SCENE");
        ImGui::SameLine();
        ImGui::TextUnformatted(SceneFile::kDefaultPath);
        ImGui::SameLine();
        if (session_.IsDirty(scene)) { ImGui::TextColored({1,0.72f,0.3f,1}, "* 未保存"); }
        else { ImGui::TextDisabled("保存済み"); }
        ImGui::BeginDisabled(session_.IsPlaying());
        if (ImGui::Button("新規")) { RequestSceneAction(SceneAction::NewScene, scene); }
        ImGui::SameLine();
        if (ImGui::Button("保存 Ctrl+S")) { SaveScene(scene); }
        ImGui::SameLine();
        if (ImGui::Button("読込")) { RequestSceneAction(SceneAction::LoadScene, scene); }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!session_.CanUndo());
        if (ImGui::Button("Undo")) { session_.Undo(scene, fileStatus_); }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!session_.CanRedo());
        if (ImGui::Button("Redo")) { session_.Redo(scene, fileStatus_); }
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (session_.IsPlaying()) {
            if (ImGui::Button("停止")) { TogglePlay(scene); }
        } else {
            if (ImGui::Button("再生 Ctrl+P")) { TogglePlay(scene); }
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(!session_.IsPlaying());
        if (session_.IsPaused()) {
            if (ImGui::Button("再開")) { session_.TogglePause(); }
        } else {
            if (ImGui::Button("一時停止")) { session_.TogglePause(); }
        }
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::BeginDisabled(!session_.IsPlaying() || !session_.IsPaused());
        if (ImGui::Button("Step")) { session_.RequestStep(); }
        ImGui::EndDisabled();
        if (!fileStatus_.empty()) { ImGui::TextWrapped("%s", fileStatus_.c_str()); }
    }
    ImGui::End();
#else
    (void)scene;
#endif
}
// 保存成功または破棄の選択が済むまで、シーンを置き換えない。
void Editor::DrawDiscardPrompt(IEditableScene& scene) {
#ifdef USE_IMGUI
    if (openDiscardPrompt_) {
        ImGui::OpenPopup("Unsaved Scene");
        openDiscardPrompt_ = false;
    }
    if (ImGui::BeginPopupModal("Unsaved Scene", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextUnformatted("Save object changes before replacing the scene?");
        if (ImGui::Button("Save and Continue")) {
            bool saved = SaveScene(scene);
            if (saved && pendingAction_ == SceneAction::Exit) {
                saved = particleEditor_.Save(scene);
                if (!saved) { fileStatus_ = "Particleプリセットを保存できません。設定値または保存先を確認してください。"; }
            }
            if (saved) {
                PerformSceneAction(scene);
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Discard and Continue")) {
            PerformSceneAction(scene);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel")) {
            pendingAction_ = SceneAction::None;
            ImGui::CloseCurrentPopup();
        }
        if (!fileStatus_.empty()) {
            ImGui::TextWrapped("%s", fileStatus_.c_str());
        }
        ImGui::EndPopup();
    }
#else
    (void)scene;
#endif
}

// 文字入力やポップアップの操作と競合しないときだけショートカットを処理する。
void Editor::HandleShortcuts(IEditableScene& scene) {
#ifdef USE_IMGUI
    const ImGuiIO& inputState = ImGui::GetIO();
    if (inputState.WantTextInput || ImGui::IsAnyItemActive() || sceneView_.IsManipulating() ||
        ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
        return;
    }
    if (inputState.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P, false)) {
        TogglePlay(scene);
    }
    if (session_.IsPlaying()) {
        return;
    }
    if (inputState.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) {
        SaveScene(scene);
    }
    if (inputState.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (inputState.KeyShift) {
            session_.Redo(scene, fileStatus_);
        } else {
            session_.Undo(scene, fileStatus_);
        }
    }
    if (inputState.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        session_.Redo(scene, fileStatus_);
    }
    if (inputState.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) {
        const SceneObjectId duplicateId = scene.DuplicateObject(selection_.Get());
        if (duplicateId != kInvalidSceneObjectId) {
            selection_.Select(duplicateId);
        }
    }
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) {
        if (scene.RemoveObject(selection_.Get())) {
            selection_.Clear();
        }
    }
#else
    (void)scene;
#endif
}

// パネルの編集を受け付け、操作が完了した状態を履歴へ記録する。
void Editor::Draw(IEditableScene* scene) {
    layout_.Draw();
    if (scene_ != scene) {
        sceneView_.ResetInteraction();
        selection_.Clear();
        fileStatus_.clear();
        pendingAction_ = SceneAction::None;
        openDiscardPrompt_ = false;
        scene_ = scene;
        if (scene != nullptr) {
            session_.Reset(*scene);
        } else {
            session_ = EditorSession{};
        }
    }
    if (scene == nullptr || !scene->SupportsObjectCollection()) {
        HierarchyPanel::Draw(scene, selection_);
        InspectorPanel::Draw({});
        return;
    }
    DrawToolbar(*scene);
    HierarchyPanel::Draw(scene, selection_);
    particleEditor_.Draw(*scene, session_.IsPlaying());
    if (particleEditor_.ConsumeFocusRequest()) {
        sceneView_.ShowScene();
        const ParticleSettings* settings = scene->GetParticleSettings();
        if (settings != nullptr) { scene->FocusEditorCamera(settings->emitterPosition); }
    }
    utilityPanels_.Draw(*scene, selection_);
    sceneView_.Draw(*scene, selection_);
    EditableObjectView selectedObject{};
    for (size_t objectIndex = 0; objectIndex < scene->GetEditableObjectCount(); ++objectIndex) {
        EditableObjectView object = scene->GetEditableObject(objectIndex);
        if (object.id != kInvalidSceneObjectId && object.id == selection_.Get()) {
            selectedObject = object;
            break;
        }
    }
    if (selectedObject.id == kInvalidSceneObjectId) {
        selection_.Clear();
    }
    InspectorPanel::Draw(selectedObject);
    DrawDiscardPrompt(*scene);
    if (!fileStatus_.empty() && fileStatus_ != loggedStatus_) {
        LogSystem::Log(fileStatus_ + "\n");
        loggedStatus_ = fileStatus_;
    }
#ifdef USE_IMGUI
    session_.RecordChanges(*scene, ImGui::IsAnyItemActive() || sceneView_.IsManipulating());
    HandleShortcuts(*scene);
    session_.RecordChanges(*scene, ImGui::IsAnyItemActive() || sceneView_.IsManipulating());
#endif
}
