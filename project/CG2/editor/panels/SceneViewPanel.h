#pragma once

#include "IEditableScene.h"

class EditorSelection;

// 描画中の画面へ選択枠と移動ギズモを重ねる。描画用リソースは所有しない。
class SceneViewPanel {
public:
    void Draw(IEditableScene& scene, EditorSelection& selection);
    void ResetInteraction();
    bool IsManipulating() const { return dragging_; }
    void SetTexture(uint64_t textureHandle) { textureHandle_ = textureHandle; }
    uint32_t GetWidth() const { return width_; }
    uint32_t GetHeight() const { return height_; }
    void ShowScene() { forceScene_ = true; }

private:
    static EditableObjectView FindObject(IEditableScene& scene, SceneObjectId objectId);
    void UpdateDrag(IEditableScene& scene, const EditorViewInfo& viewInfo, const Vector2& mousePosition);

    bool dragging_ = false;
    SceneObjectId draggedObject_ = kInvalidSceneObjectId;
    Vector3 dragStartPosition_{};
    Vector3 dragAxis_{};
    float dragStartDistance_ = 0;
    float snapDistance_ = 1;
    bool showGrid_ = true;
    uint64_t textureHandle_ = 0;
    uint32_t width_ = 800;
    uint32_t height_ = 500;
    bool gameView_ = false;
    bool forceScene_ = false;
};
