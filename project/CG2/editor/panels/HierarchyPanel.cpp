#include "HierarchyPanel.h"
#include <string>
#include "../EditorSelection.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// 追加・複製・削除を一覧表示の前に行い、古い参照を使わずに選択を更新する。
void HierarchyPanel::Draw(IEditableScene* scene, EditorSelection& selection) {
#ifdef USE_IMGUI
    if (ImGui::Begin("Hierarchy")) {
        ImGui::BeginDisabled(!scene || !scene->SupportsObjectCollection());
        if (ImGui::Button("Add Plane")) {
            const SceneObjectId id = scene->AddObject("plane.gltf");
            if (id != kInvalidSceneObjectId) { selection.Select(id); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Axis")) {
            const SceneObjectId id = scene->AddObject("axis.obj");
            if (id != kInvalidSceneObjectId) { selection.Select(id); }
        }
        ImGui::BeginDisabled(selection.Get() == kInvalidSceneObjectId);
        if (ImGui::Button("Duplicate")) {
            const SceneObjectId id = scene->DuplicateObject(selection.Get());
            if (id != kInvalidSceneObjectId) { selection.Select(id); }
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            if (scene->RemoveObject(selection.Get())) { selection.Clear(); }
        }
        ImGui::EndDisabled();
        ImGui::EndDisabled();
        if (ImGui::Button("Clear Selection")) {
            selection.Clear();
        }
        ImGui::Separator();
        if (scene) {
            ImGui::Text("Objects: %zu", scene->GetEditableObjectCount());
            for (size_t objectIndex = 0; objectIndex < scene->GetEditableObjectCount(); ++objectIndex) {
                const EditableObjectView object = scene->GetEditableObject(objectIndex);
                if (object.id == kInvalidSceneObjectId) { continue; }
                // 64ビットIDを途中で切り捨てず、同名のオブジェクトも別のUI要素として識別する。
                const std::string id = std::to_string(object.id);
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
