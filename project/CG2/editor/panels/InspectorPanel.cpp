#include "InspectorPanel.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cstdio>

namespace {
	// 必須のTransformを編集する。リセット時は位置・回転をゼロ、拡縮を一に戻す。
	void DrawTransformComponent(TransformData* transform, bool positionOnly) {
		if (transform == nullptr) {
			return;
		}
		if (!ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}
		ImGui::DragFloat3("Position", &transform->translate.x, 0.01f);
		// 点光源とEmitterは位置を使う。未実装の回転・拡縮は編集不可として明示する。
		ImGui::BeginDisabled(positionOnly);
		ImGui::SliderAngle("Rotation X", &transform->rotate.x);
		ImGui::SliderAngle("Rotation Y", &transform->rotate.y);
		ImGui::SliderAngle("Rotation Z", &transform->rotate.z);
		ImGui::DragFloat3("Scale", &transform->scale.x, 0.01f);
		ImGui::EndDisabled();
		if (ImGui::Button("Reset Transform")) {
			*transform = {{1, 1, 1}, {}, {}};
		}
	}

	// 描画Componentの有無と設定を編集する。削除してもモデル設定は保持する。
	void DrawMeshRendererComponent(const EditableObjectView& object) {
		if (object.hasMeshRenderer != nullptr) {
			if (!*object.hasMeshRenderer) {
				if (ImGui::Button("Add Mesh Renderer")) {
					*object.hasMeshRenderer = true;
				}
				return;
			}
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
void InspectorPanel::Draw(const EditableObjectView& object, bool* particleEditorOpen) {
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
			// 子のTransformは親に対する値で、ギズモはワールド座標を使う。
			if (object.parentId != kInvalidSceneObjectId) {
				ImGui::TextUnformatted("Transform / 親に対するローカル値");
			}
			DrawTransformComponent(object.transform, object.type == SceneObjectType::PointLight || object.type == SceneObjectType::Particle);
			if (object.type == SceneObjectType::Model || object.type == SceneObjectType::Empty) {
				DrawMeshRendererComponent(object);
			}
			if (object.type == SceneObjectType::PointLight) {
				if (object.lightIntensity != nullptr) {
					// 光源の位置は共通Transform、色と減衰はLight Componentが所有する。
					if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
						ImGui::ColorEdit4("Light Color", &object.color->x);
						ImGui::DragFloat("Intensity", object.lightIntensity, 0.05f, 0, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
						ImGui::DragFloat("Radius", object.lightRadius, 0.1f, 0.01f, 1000, "%.2f", ImGuiSliderFlags_AlwaysClamp);
						ImGui::DragFloat("Decay", object.lightDecay, 0.05f, 0.01f, 100, "%.2f", ImGuiSliderFlags_AlwaysClamp);
					}
				}
			}
			if (object.type == SceneObjectType::Particle) {
				ImGui::TextWrapped("発生設定はParticle Editorで編集できます。設定はシーンに保存されます。");
				if (particleEditorOpen != nullptr) {
					if (ImGui::Button("Particle Editorを開く")) {
						*particleEditorOpen = true;
					}
				}
			}
		}
	}
	ImGui::End();
#else
	(void)object;
	(void)particleEditorOpen;
#endif
}
