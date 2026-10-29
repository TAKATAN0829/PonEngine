#pragma once

#include "IEditableScene.h"

// Selection never owns or keeps a pointer to a scene object.
class EditorSelection {
public:
    SceneObjectId Get() const { return selected_; }
    void Select(SceneObjectId id) { selected_ = id; }
    void Clear() { selected_ = kInvalidSceneObjectId; }

private:
    SceneObjectId selected_ = kInvalidSceneObjectId;
};
