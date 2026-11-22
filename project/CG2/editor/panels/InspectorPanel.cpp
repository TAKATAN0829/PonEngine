#include "InspectorPanel.h"
#include "ModelCollection.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=================================================================================================//
// 描画処理

void InspectorPanel::Draw(SceneModel* model) {
#ifdef USE_IMGUI

	if (ImGui::Begin("Inspector")) {
		if (model == nullptr) {
			ImGui::TextUnformatted("Select a model in Hierarchy");
		} else {
			// 名前
			ImGui::TextUnformatted(model->name.c_str());

			// 位置・回転・拡縮
			if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
				TransformData& transform = model->transform;
				ImGui::DragFloat3("Position", &transform.translate.x, 0.01f);
				ImGui::SliderAngle("Rotation X", &transform.rotate.x);
				ImGui::SliderAngle("Rotation Y", &transform.rotate.y);
				ImGui::SliderAngle("Rotation Z", &transform.rotate.z);
				ImGui::DragFloat3("Scale", &transform.scale.x, 0.01f);
			}

			// 見た目
			if (ImGui::CollapsingHeader("Appearance", ImGuiTreeNodeFlags_DefaultOpen)) {
				ImGui::ColorEdit4("Color", &model->color.x);
				ImGui::Checkbox("Enable Lighting", &model->enableLighting);
				ImGui::DragFloat("Shininess", &model->shininess, 0.1f, 1.0f, 500.0f);
			}
		}
	}
	ImGui::End();

#else

	(void)model;

#endif
}
