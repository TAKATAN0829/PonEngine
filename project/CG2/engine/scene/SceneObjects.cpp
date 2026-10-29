#include "SceneObjects.h"

#include <array>
#include <cmath>
#include <limits>
#include <unordered_set>

// 読み込み済みのモデルだけを許可し、不明なアセットへの参照を防ぐ。
bool SceneObjects::IsSupportedModel(const std::string& model) {
    return model == "plane.gltf" || model == "axis.obj";
}

// 保存や復元に使えない数値・名前・ID・モデル指定を検出する。
bool SceneObjects::Validate(const SceneObjectData& object) {
    const TransformData& transform = object.transform;
    const Vector4& color = object.color;
    const std::array<float, 14> values = {transform.scale.x, transform.scale.y, transform.scale.z,
        transform.rotate.x, transform.rotate.y, transform.rotate.z,
        transform.translate.x, transform.translate.y, transform.translate.z,
        color.x, color.y, color.z, color.w, object.shininess};
    for (float value : values) {
        if (!std::isfinite(value)) {
            return false;
        }
    }
    if (object.id == kInvalidSceneObjectId || object.id == (std::numeric_limits<SceneObjectId>::max)()) {
        return false;
    }
    if (object.name.empty() || object.name.size() > 256 || object.name.find('\0') != std::string::npos) {
        return false;
    }
    if (!IsSupportedModel(object.model)) {
        return false;
    }
    if (object.shininess < 1 || object.shininess > 500) {
        return false;
    }
    return true;
}

// シーンが所有する値への借用参照を返す。追加・削除後は取得し直すこと。
EditableObjectView SceneObjects::GetEditableObject(size_t index) {
    if (index >= objects_.size()) { return {}; }
    SceneObjectData& object = objects_[index];
    return {object.id, object.name.c_str(), &object.transform, &object.color, &object.enableLighting,
        &object.shininess, &object.model, &object.useRootNodeMatrix, &object.name,
        &object.active, &object.hasMeshRenderer};
}

// 新しいIDと既定の設定を持つモデルを追加する。削除済みIDは再利用しない。
SceneObjectId SceneObjects::AddObject(const std::string& model) {
    if (!IsSupportedModel(model) || objects_.size() >= kMaxObjects ||
        nextId_ == (std::numeric_limits<SceneObjectId>::max)()) { return kInvalidSceneObjectId; }
    SceneObjectData object;
    object.id = nextId_;
    object.model = model;
    object.name = model + " #" + std::to_string(object.id);
    objects_.push_back(std::move(object));
    return nextId_++;
}

// 設定を値として複製し、新しいIDを付けて元のモデルから独立させる。
SceneObjectId SceneObjects::DuplicateObject(SceneObjectId id) {
    const size_t objectIndex = FindObjectIndex(id);
    if (objectIndex == objects_.size() || objects_.size() >= kMaxObjects ||
        nextId_ == (std::numeric_limits<SceneObjectId>::max)()) { return kInvalidSceneObjectId; }
    SceneObjectData copy = objects_[objectIndex];
    copy.id = nextId_;
    copy.name = copy.model + " #" + std::to_string(copy.id);
    objects_.push_back(std::move(copy));
    return nextId_++;
}

// 指定IDだけを削除し、残ったオブジェクトのIDは変更しない。
bool SceneObjects::RemoveObject(SceneObjectId id) {
    const size_t objectIndex = FindObjectIndex(id);
    if (objectIndex == objects_.size()) { return false; }
    objects_.erase(objects_.begin() + objectIndex);
    return true;
}

// IDに対応する位置を検索する。見つからない場合は要素数を返す。
size_t SceneObjects::FindObjectIndex(SceneObjectId objectId) const {
    for (size_t objectIndex = 0; objectIndex < objects_.size(); ++objectIndex) {
        if (objects_[objectIndex].id == objectId) {
            return objectIndex;
        }
    }
    return objects_.size();
}

// 全件の検証が済んでから一覧を置き換え、不正データの部分反映を防ぐ。
bool SceneObjects::ReplaceObjects(std::vector<SceneObjectData> objects, std::string& error) {
    error.clear();
    if (objects.size() > kMaxObjects) { error = "Too many scene objects."; return false; }
    std::unordered_set<SceneObjectId> ids;
    SceneObjectId nextId = nextId_;
    for (const SceneObjectData& object : objects) {
        if (!Validate(object) || !ids.insert(object.id).second) {
            error = "Invalid object data, unsupported model, or duplicate ID.";
            return false;
        }
        if (object.id >= nextId) {
            nextId = object.id + 1;
        }
    }
    objects_ = std::move(objects);
    nextId_ = nextId;
    return true;
}
