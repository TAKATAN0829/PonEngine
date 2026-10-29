#include "HierarchyPanel.h"
#include <string>
#include "../EditorSelection.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void HierarchyPanel::Draw(IEditableScene* scene, EditorSelection& selection) {
#ifdef USE_IMGUI
    if (ImGui::Begin("Hierarchy")) {
        if (ImGui::Button("Clear Selection")) {
            selection.Clear();
        }
        ImGui::Separator();
        if (scene) {
            for (size_t i = 0; i < scene->GetEditableObjectCount(); ++i) {
                const auto object = scene->GetEditableObject(i);
                if (object.id == kInvalidSceneObjectId) { continue; }
                // Use the full numeric ID as text to avoid truncating 64-bit IDs.
                const auto id = std::to_string(object.id);
                ImGui::PushID(id.c_str());
                if (ImGui::Selectable(object.name, selection.Get() == object.id)) {
                    selection.Select(object.id);
                }
                ImGui::PopID();
            }
        } else {
            ImGui::TextUnformatted("No editable scene");
        }
    }
    ImGui::End();
#else
    (void)scene;
    (void)selection;
#endif
}
