#include "Editor.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "SceneFile.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void Editor::Draw(IEditableScene* scene) {
    if (scene_ != scene) {
        selection_.Clear();
        fileStatus_.clear();
        scene_ = scene;
    }

#ifdef USE_IMGUI
    if (ImGui::Begin("Scene")) {
        ImGui::TextWrapped("%s", SceneFile::kDefaultPath);
        ImGui::BeginDisabled(scene == nullptr);
        if (ImGui::Button("Save Scene")) {
            std::string error;
            fileStatus_ = SceneFile::Save(*scene, SceneFile::kDefaultPath, error) ? "Scene saved." : error;
        }
        ImGui::SameLine();
        if (ImGui::Button("Load Scene")) {
            std::string error;
            if (SceneFile::Load(*scene, SceneFile::kDefaultPath, error)) {
                selection_.Clear();
                fileStatus_ = "Scene loaded.";
            } else {
                fileStatus_ = error;
            }
        }
        ImGui::EndDisabled();
        if (!fileStatus_.empty()) { ImGui::TextWrapped("%s", fileStatus_.c_str()); }
    }
    ImGui::End();
#endif

    HierarchyPanel::Draw(scene, selection_);

    EditableObjectView selected{};
    if (scene) {
        for (size_t i = 0; i < scene->GetEditableObjectCount(); ++i) {
            auto object = scene->GetEditableObject(i);
            if (object.id != kInvalidSceneObjectId && object.id == selection_.Get()) {
                selected = object;
                break;
            }
        }
    }
    if (selected.id == kInvalidSceneObjectId) {
        selection_.Clear();
    }
    InspectorPanel::Draw(selected);
}
