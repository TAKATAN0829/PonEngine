#pragma once

#include <cstddef>
#include <cstdint>
#include "EngineStructs.h"

// SceneObjectIDはuint64と同じ扱い
using SceneObjectId = uint64_t;

inline constexpr SceneObjectId kInvalidSceneObjectId = 0;

// モデル1つの編集に必要な情報
struct EditableObjectView {
    SceneObjectId id = kInvalidSceneObjectId; // id
    const char* name = "";                    // 名前
    TransformData* transform = nullptr;       // TransformData
    Vector4* color = nullptr;                 // 色
    bool* enableLighting = nullptr;           // ライティング
    float* shininess = nullptr;               // 光沢の強さ
};

class IEditableScene {
public:
    virtual ~IEditableScene() = default;
    virtual size_t GetEditableObjectCount() const = 0;
    virtual EditableObjectView GetEditableObject(size_t index) = 0;
};
