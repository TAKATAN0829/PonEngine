#pragma once

#include "IEditableScene.h"

// 編集用データだけを所有する。配列の移動や削除があっても、残る要素のIDは変えない。
class SceneObjects : public IEditableScene {
public:
    static constexpr size_t kMaxObjects = 10000;
    static bool IsSupportedModel(const std::string& model);
    static bool Validate(const SceneObjectData& object);

    size_t GetEditableObjectCount() const override { return objects_.size(); }
    EditableObjectView GetEditableObject(size_t index) override;
    bool SupportsObjectCollection() const override { return true; }
    SceneObjectId AddObject(const std::string& model) override;
    SceneObjectId DuplicateObject(SceneObjectId id) override;
    bool RemoveObject(SceneObjectId id) override;
    std::vector<SceneObjectData> CaptureObjects() const override { return objects_; }
    bool ReplaceObjects(std::vector<SceneObjectData> objects, std::string& error) override;
    const std::vector<SceneObjectData>& GetObjects() const { return objects_; }

private:
    size_t FindObjectIndex(SceneObjectId objectId) const;
    std::vector<SceneObjectData> objects_;
    SceneObjectId nextId_ = 1; // 読込やUndoの後も巻き戻さず、IDの再利用を防ぐ。
};
