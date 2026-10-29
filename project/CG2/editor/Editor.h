#pragma once

#include "EditorSelection.h"
#include "EditorSession.h"
#include "panels/SceneViewPanel.h"
#include <string>

// 各パネルをまとめ、編集・保存・再生・終了確認の操作順序を管理する。
class Editor {
public:
    void Draw(IEditableScene* scene);
    bool IsPlaying() const { return session_.IsPlaying(); }
    bool ShouldSimulate();
    void RequestExit(IEditableScene* scene);
    bool ShouldExit() const { return exitApproved_; }

private:
    enum class SceneAction { None, NewScene, LoadScene, Exit };
    bool SaveScene(IEditableScene& scene);
    void RequestSceneAction(SceneAction action, IEditableScene& scene);
    void PerformSceneAction(IEditableScene& scene);
    void DrawToolbar(IEditableScene& scene);
    void DrawDiscardPrompt(IEditableScene& scene);
    void HandleShortcuts(IEditableScene& scene);
    void TogglePlay(IEditableScene& scene);
    IEditableScene* scene_ = nullptr; // 借用ポインタ。シーン切替の検出に使い、破棄は行わない。
    EditorSelection selection_;
    EditorSession session_;
    SceneViewPanel sceneView_; // 画面上の選択とドラッグ状態を管理する。
    std::string fileStatus_; // 保存や読込の結果を表示するメッセージ。
    SceneAction pendingAction_ = SceneAction::None; // 保存確認待ちの操作。
    bool openDiscardPrompt_ = false; // 次の描画時に確認ダイアログを開く。
    bool exitApproved_ = false; // 終了確認が済んだことをEngineへ伝える。
};
