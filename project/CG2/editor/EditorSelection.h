#pragma once

#include "IEditableScene.h"

// 選択対象はIDだけで保持し、削除される可能性のあるオブジェクトへの参照を残さない。
class EditorSelection {
public:
    SceneObjectId Get() const { return selected_; }
    void Select(SceneObjectId id) { selected_ = id; }
    void Clear() { selected_ = kInvalidSceneObjectId; }

private:
    SceneObjectId selected_ = kInvalidSceneObjectId;
};
