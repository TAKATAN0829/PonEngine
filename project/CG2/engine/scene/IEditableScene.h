#pragma once

#include <cstddef>
#include <cstdint>
#include "EngineStructs.h"

using SceneObjectId = uint64_t;
inline constexpr SceneObjectId kInvalidSceneObjectId = 0;

// Borrowed views are valid only during the current editor frame.
// The scene owns the data; this contract has no dependency on ImGui.
struct EditableObjectView {
    SceneObjectId id = kInvalidSceneObjectId;
    const char* name = "";
    TransformData* transform = nullptr;
    Vector4* color = nullptr;
    bool* enableLighting = nullptr;
    float* shininess = nullptr;
};

class IEditableScene {
public:
    virtual ~IEditableScene() = default;
    virtual size_t GetEditableObjectCount() const = 0;
    virtual EditableObjectView GetEditableObject(size_t index) = 0;
};
