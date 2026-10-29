#include "InspectorPanel.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void InspectorPanel::Draw(const EditableObjectView& object) {
#ifdef USE_IMGUI
    if (ImGui::Begin("Inspector")) {
        if (object.id == kInvalidSceneObjectId) {
            ImGui::TextUnformatted("Select an object in Hierarchy");
        } else {
            ImGui::TextUnformatted(object.name);
            if (object.transform && ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                auto& transform = *object.transform;
                ImGui::DragFloat3("Position", &transform.translate.x, 0.01f);
                ImGui::SliderAngle("Rotation X", &transform.rotate.x);
                ImGui::SliderAngle("Rotation Y", &transform.rotate.y);
                ImGui::SliderAngle("Rotation Z", &transform.rotate.z);
                ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);
            }
            if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
                if (object.color) { ImGui::ColorEdit4("Color", &object.color->x); }
                if (object.enableLighting) { ImGui::Checkbox("Enable Lighting", object.enableLighting); }
                if (object.shininess) { ImGui::DragFloat("Shininess", object.shininess, 0.1f, 1.0f, 500.0f); }
            }
        }
    }
    ImGui::End();
#else
    (void)object;
#endif
}
