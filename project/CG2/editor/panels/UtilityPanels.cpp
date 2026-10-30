#include "UtilityPanels.h"
#include "../EditorSelection.h"
#include "LogSystem.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// 読込済みのモデルを選択対象へ適用し、Consoleには保存・読込やシェーダーの実ログを出す。
void UtilityPanels::Draw(IEditableScene& scene, const EditorSelection& selection) {
#ifdef USE_IMGUI
    if (ImGui::Begin("Project")) {
        ImGui::TextUnformatted("Models / 選択中のオブジェクトへ適用");
        EditableObjectView selectedObject{};
        for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
            const EditableObjectView object = scene.GetEditableObject(objectIndex);
            if (object.id == selection.Get()) { selectedObject = object; break; }
        }
        ImGui::BeginDisabled(selectedObject.model == nullptr);
        if (ImGui::Button("plane.gltf")) { *selectedObject.model = "plane.gltf"; }
        ImGui::SameLine();
        if (ImGui::Button("axis.obj")) { *selectedObject.model = "axis.obj"; }
        ImGui::EndDisabled();
        ImGui::Separator();
        ImGui::TextUnformatted("Particles / resources/particles/Default.ponparticle");
        if (ImGui::Button("Particle Editorを表示")) { ImGui::SetWindowFocus("Particle Editor"); }
    }
    ImGui::End();
    if (ImGui::Begin("Console")) {
        ImGui::Text("Engine log / %zu messages", LogSystem::GetRecentMessages().size());
        ImGui::Separator();
        ImGui::BeginChild("ConsoleMessages");
        for (const std::string& message : LogSystem::GetRecentMessages()) {
            ImGui::TextUnformatted(message.c_str());
        }
        ImGui::EndChild();
    }
    ImGui::End();
#else
    (void)scene;
    (void)selection;
#endif
}
