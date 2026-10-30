#pragma once
#include "IEditableScene.h"

// 発生設定とプレビューを管理し、粒子の実行データはGameSceneへ任せる。
class ParticleEditorPanel {
public:
    void Draw(IEditableScene& scene, bool gamePlaying);
    bool IsDirty(IEditableScene& scene) const;
    bool Save(IEditableScene& scene);
    bool ConsumeFocusRequest();
private:
    void ApplyPendingAction(IEditableScene& scene);
    IEditableScene* scene_ = nullptr;
    ParticleSettings savedSettings_;
    ParticlePreviewState preview_;
    std::string status_;
    int pendingAction_ = 0; // 0:なし、1:読込、2:既定値へ戻す。
    bool openPrompt_ = false;
    bool focusRequested_ = false;
};
