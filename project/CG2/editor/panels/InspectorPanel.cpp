#include "InspectorPanel.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cstdio>

namespace {
// 必須のTransformを編集する。リセット時は位置・回転をゼロ、拡縮を一に戻す。
void DrawTransformComponent(TransformData* transform) {
    if (transform == nullptr || !ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    ImGui::DragFloat3("Position", &transform->translate.x, 0.01f);
    ImGui::SliderAngle("Rotation X", &transform->rotate.x);
    ImGui::SliderAngle("Rotation Y", &transform->rotate.y);
    ImGui::SliderAngle("Rotation Z", &transform->rotate.z);
    ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f);
    if (ImGui::Button("Reset Transform")) {
        *transform = {{1, 1, 1}, {}, {}};
    }
}

// 描画Componentの有無と設定を編集する。削除してもモデル設定は保持する。
void DrawMeshRendererComponent(const EditableObjectView& object) {
    if (object.hasMeshRenderer != nullptr && !*object.hasMeshRenderer) {
        if (ImGui::Button("Add Mesh Renderer")) {
            *object.hasMeshRenderer = true;
        }
        return;
    }
    if (!ImGui::CollapsingHeader("Mesh Renderer", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }
    if (object.hasMeshRenderer != nullptr) {
        if (ImGui::Button("Remove Mesh Renderer")) {
            *object.hasMeshRenderer = false;
            return;
        }
    }
    if (object.model != nullptr) {
        const char* models[] = {"plane.gltf", "axis.obj"};
        int modelIndex = 0;
        if (*object.model == "axis.obj") {
            modelIndex = 1;
        }
        if (ImGui::Combo("Model", &modelIndex, models, IM_ARRAYSIZE(models))) {
            *object.model = models[modelIndex];
        }
    }
    if (object.useRootNodeMatrix != nullptr) {
        ImGui::Checkbox("Use Root Node Matrix", object.useRootNodeMatrix);
    }
    if (object.color != nullptr) {
        ImGui::ColorEdit4("Color", &object.color->x);
    }
    if (object.enableLighting != nullptr) {
        ImGui::Checkbox("Enable Lighting", object.enableLighting);
    }
    if (object.shininess != nullptr) {
        ImGui::DragFloat("Shininess", object.shininess, 0.1f, 1.0f, 500.0f, "%.1f", ImGuiSliderFlags_AlwaysClamp);
    }
}
}
#endif

// 選択対象の名前・有効状態・各Componentを編集し、未選択時は案内を表示する。
void InspectorPanel::Draw(const EditableObjectView& object) {
#ifdef USE_IMGUI
    if (ImGui::Begin("Inspector")) {
        if (object.id == kInvalidSceneObjectId) {
            ImGui::TextUnformatted("Select an object in Hierarchy");
        } else {
            if (object.editableName != nullptr) {
                char nameBuffer[257]{};
                std::snprintf(nameBuffer, sizeof(nameBuffer), "%s", object.editableName->c_str());
                if (ImGui::InputText("Name", nameBuffer, sizeof(nameBuffer))) {
                    if (nameBuffer[0] != '\0') {
                        *object.editableName = nameBuffer;
                    }
                }
            } else {
                ImGui::TextUnformatted(object.name);
            }
            if (object.active != nullptr) {
                ImGui::Checkbox("Active", object.active);
            }
            DrawTransformComponent(object.transform);
            DrawMeshRendererComponent(object);
        }
    }
    ImGui::End();
#else
    (void)object;
#endif
}
