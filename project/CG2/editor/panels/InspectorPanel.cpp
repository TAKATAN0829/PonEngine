#include "../EditorProperties.h"
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
		const bool expanded = EditorProperties::Header("Transform","位置・回転・拡縮は親に対するローカル値です。見出しを右クリックするとリセットできます。");
		if (ImGui::BeginPopupContextItem("TransformActions")) {
			if (ImGui::MenuItem("リセット")) {
				*transform = {{1,1,1},{},{}};
			}
			ImGui::EndPopup();
		}
		if (!expanded) {
			return;
		}
		if (!EditorProperties::Begin("TransformFields")) {
			return;
		}
		EditorProperties::Vector("Position",&transform->translate,0.01f);
		ImGui::BeginDisabled(positionOnly);
		Vector3 degrees = MathUtility::Multiply(57.2957795f,transform->rotate);
		if (EditorProperties::Vector("Rotation",&degrees,0.5f,-180,180)) {
			transform->rotate = MathUtility::Multiply(0.0174532925f,degrees);
		}
		EditorProperties::Vector("Scale",&transform->scale,0.01f);
		ImGui::EndDisabled();
		EditorProperties::End();
	}

	// メイン指定はシーンを通し、複数カメラが同時にメインにならないようにする。
	void DrawCameraComponent(const EditableObjectView& object, IEditableScene* scene) {
		if (object.camera == nullptr) {
			return;
		}
		if (!EditorProperties::Header("Camera","GameViewはメインカメラを使用します。拡縮は視野へ反映しません。")) {
			return;
		}
		if (!EditorProperties::Begin("CameraFields")) {
			return;
		}
		bool mainCamera = object.mainCamera;
		if (EditorProperties::Toggle("Main Camera", &mainCamera)) {
			if (scene != nullptr) {
				if (mainCamera) {
					scene->SetMainCamera(object.id);
				}
				else {
					scene->SetMainCamera(kInvalidSceneObjectId);
				}
			}
		}
		float degrees = object.camera->fieldOfView * 57.2957795f;
		if (EditorProperties::Float("Field of View",&degrees,0.5f,1,179)) {
			object.camera->fieldOfView = degrees * 0.0174532925f;
		}
		EditorProperties::Float("Near Clip", &object.camera->nearClip, 0.01f, 0.001f, object.camera->farClip * 0.99f,"%.3f");
		EditorProperties::Float("Far Clip", &object.camera->farClip, 1, object.camera->nearClip * 1.01f, 1000000,"%.1f");
		EditorProperties::End();
	}

	// 描画Componentの有無と設定を編集する。削除してもモデル設定は保持する。
	void DrawMeshRendererComponent(const EditableObjectView& object) {
		if (object.hasMeshRenderer != nullptr) {
			if (!*object.hasMeshRenderer) {
				return;
			}
		}
		const bool expanded = EditorProperties::Header("Mesh Renderer","モデルと見た目の共通設定です。右クリックでComponentを削除できます。");
		if (ImGui::BeginPopupContextItem("MeshRendererActions")) {
			if (object.hasMeshRenderer != nullptr) {
				if (ImGui::MenuItem("Componentを削除")) {
					*object.hasMeshRenderer = false;
				}
			}
			ImGui::EndPopup();
		}
		if (object.hasMeshRenderer != nullptr) {
			if (!*object.hasMeshRenderer) {
				return;
			}
		}
		if (!expanded) {
			return;
		}
		if (!EditorProperties::Begin("MeshRendererFields")) {
			return;
		}
		if (object.model != nullptr) {
			const char* models[] = {"plane.gltf", "axis.obj"};
			// Projectから適用した任意モデルも、実際の参照先を表示する。
			EditorProperties::Row("Model");
			if (ImGui::BeginCombo("##Model", object.model->c_str())) {
				for (const char* modelName : models) {
					if (ImGui::Selectable(modelName, *object.model == modelName)) {
						*object.model = modelName;
						if (object.materials != nullptr) {
							object.materials->clear();
						}
						if (object.animation != nullptr) {
							*object.animation = {};
						}
					}
				}
				ImGui::EndCombo();
			}
		}
		if (object.useRootNodeMatrix != nullptr) {
			EditorProperties::Toggle("Root Matrix", object.useRootNodeMatrix);
		}
		if (object.color != nullptr) {
			EditorProperties::Color("Color", object.color);
		}
		if (object.enableLighting != nullptr) {
			EditorProperties::Toggle("Lighting", object.enableLighting);
		}
		if (object.shininess != nullptr) {
			EditorProperties::Float("Shininess", object.shininess, 0.1f, 1.0f, 500.0f,"%.1f");
		}
		EditorProperties::End();
	}
}
#endif

// 選択対象の名前・有効状態・各Componentを編集し、未選択時は案内を表示する。
void InspectorPanel::Draw(const EditableObjectView& object, bool* particleEditorOpen, IEditableScene* scene) {
#ifdef USE_IMGUI
	if (ImGui::Begin("Inspector")) {
		if (object.id == kInvalidSceneObjectId) {
			ImGui::Spacing();
			ImGui::TextDisabled("オブジェクトを選択");
			ImGui::TextWrapped("HierarchyまたはSceneViewで対象を選択すると、Componentを編集できます。");
		} else {
			if (object.active != nullptr) {
				ImGui::Checkbox("##ObjectActive",object.active);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("オブジェクト全体の有効・無効");
				}
				ImGui::SameLine();
			}
			if (object.editableName != nullptr) {
				char nameBuffer[257]{};
				std::snprintf(nameBuffer,sizeof(nameBuffer),"%s",object.editableName->c_str());
				ImGui::SetNextItemWidth(-1);
				if (ImGui::InputText("##ObjectName",nameBuffer,sizeof(nameBuffer))) {
					if (nameBuffer[0] != '\0') {
						*object.editableName = nameBuffer;
					}
				}
			} else {
				ImGui::TextUnformatted(object.name);
			}
			ImGui::TextDisabled("Object  /  ID %llu",static_cast<unsigned long long>(object.id));
			ImGui::Separator();
			DrawTransformComponent(object.transform, object.type == SceneObjectType::PointLight || object.type == SceneObjectType::Particle);
			DrawMotionComponents(object, scene);
			DrawColliderComponent(object);
			DrawBodyComponent(object);
			if (object.type == SceneObjectType::Model || object.type == SceneObjectType::Empty) {
				DrawMeshRendererComponent(object);
				if (scene != nullptr) {
					if (object.hasMeshRenderer != nullptr) {
						if (*object.hasMeshRenderer) {
							DrawMaterials(object, *scene);
							DrawAnimation(object, *scene);
						}
					}
				}
			}
			if (object.type == SceneObjectType::PointLight) {
				if (object.lightIntensity != nullptr) {
					// 光源の位置は共通Transform、色と減衰はLight Componentが所有する。
					if (EditorProperties::Header("Point Light","位置はTransform、色と減衰はLight Componentが所有します。")) {
						if (EditorProperties::Begin("LightFields")) {
							EditorProperties::Color("Color",object.color);
							EditorProperties::Float("Intensity",object.lightIntensity,0.05f,0,100);
							EditorProperties::Float("Radius",object.lightRadius,0.1f,0.01f,1000);
							EditorProperties::Float("Decay",object.lightDecay,0.05f,0.01f,100);
							EditorProperties::End();
						}
					}
				}
			}
			if (object.type == SceneObjectType::Camera) {
				DrawCameraComponent(object, scene);
			}
			if (object.type == SceneObjectType::Particle) {
				ImGui::TextDisabled("Particle Emitter");
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
	(void)scene;
#endif
}
