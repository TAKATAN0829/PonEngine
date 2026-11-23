#include "HierarchyPanel.h"
#include "../EditorSelection.h"
#include "ModelCollection.h"
#include <string>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=================================================================================================//
// 描画処理

void HierarchyPanel::Draw(ModelCollection* models, EditorSelection& selection) {
#ifdef USE_IMGUI

	if (ImGui::Begin("Hierarchy")) {
		// 選択解除
		if (ImGui::Button("Clear Selection")) {
			selection.Clear();
		}
		ImGui::Separator();

		if (models != nullptr) {

			// モデル追加
			if (ImGui::Button("Add Plane")) {
				const ModelId id = models->AddModel(
					"Plane",
					"plane.gltf",
					"resources/uvChecker.png"
				);
				selection.Select(id);
			}
			
			// モデル追加
			if (ImGui::Button("Add Axis")) {
				const ModelId id = models->AddModel(
					"Axis",
					"axis.obj",
					""
				);
				selection.Select(id);
			}

			// 選択モデル削除
			const ModelId selectedId = selection.Get();
			SceneModel* selectedModel = models->FindModel(selectedId);

			ImGui::BeginDisabled(selectedModel == nullptr);
			if (ImGui::Button("Delete Selected")) {
				models->RemoveModel(selectedId);
				selection.Clear();
			}
			ImGui::EndDisabled();

			ImGui::Separator();

			// モデル一覧
			for (const SceneModel& model : models->GetModels()) {
				if (model.id == 0) {
					continue;
				}

				// 64ビットのIDを切り詰めずに扱うため、文字列として渡す。
				const std::string id = std::to_string(model.id);
				ImGui::PushID(id.c_str());
				if (ImGui::Selectable(model.name.c_str(), selection.Get() == model.id)) {
					selection.Select(model.id);
				}
				ImGui::PopID();
			}
		} else {
			ImGui::TextUnformatted("No model collection");
		}
	}
	ImGui::End();

#else

	(void)models;
	(void)selection;

#endif
}
