#pragma once

#include "IEditableScene.h"

// 編集履歴と再生前の設定を所有する。ImGuiやDX12には依存しない。
class EditorSession {
public:
    void Reset(IEditableScene& scene);
    void RecordChanges(IEditableScene& scene, bool interactionActive);
    bool Undo(IEditableScene& scene, std::string& error);
    bool Redo(IEditableScene& scene, std::string& error);
    bool CanUndo() const;
    bool CanRedo() const;
    bool IsDirty(const IEditableScene& scene) const;
    void MarkSaved(const IEditableScene& scene);

    bool BeginPlay(IEditableScene& scene, std::string& error);
    bool Stop(IEditableScene& scene, std::string& error);
    bool IsPlaying() const { return playing_; }
    bool IsPaused() const { return paused_; }
    void TogglePause();
    void RequestStep();
    bool ShouldSimulate();

private:
    static bool AreObjectsEqual(const SceneObjectData& firstObject, const SceneObjectData& secondObject);
    static bool AreScenesEqual(const std::vector<SceneObjectData>& firstScene,
        const std::vector<SceneObjectData>& secondScene);
    static constexpr size_t kMaxHistoryStates = 64;

    std::vector<std::vector<SceneObjectData>> history_; // GPUリソースを含まない設定の履歴。
    size_t historyIndex_ = 0; // 現在のシーンに対応する履歴位置。
    std::vector<SceneObjectData> savedState_; // 未保存判定の基準となる設定。
    std::vector<SceneObjectData> playSnapshot_; // 停止時に戻す、再生開始直前の設定。
    bool playing_ = false;
    bool paused_ = false;
    bool stepRequested_ = false; // 一回の更新で消費するステップ要求。
};
