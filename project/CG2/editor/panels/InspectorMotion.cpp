#include "../EditorProperties.h"
#include "InspectorPanel.h"
#include "SceneCollision.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"

namespace {
	void DrawRotation(SceneMotionSettings& settings) {
		if (!settings.hasRotation) {
			return;
		}
		const bool expanded = EditorProperties::Header("Rotation","各ローカルEuler角を度/秒で更新します。親の回転も引き継ぎます。");
		if (ImGui::BeginPopupContextItem("RotationComponentMenu")) {
			if (ImGui::MenuItem("Componentを削除")) {
				settings.hasRotation = false;
			}
			ImGui::EndPopup();
		}
		if (!settings.hasRotation) {
			return;
		}
		if (!expanded) {
			return;
		}
		if (!EditorProperties::Begin("RotationFields")) {
			return;
		}
		EditorProperties::Toggle("Enabled", &settings.rotationEnabled);
		EditorProperties::Vector("Speed (deg/s)",&settings.rotationDegreesPerSecond,1,-3600,3600);
		EditorProperties::End();
	}

	void DrawMovement(SceneMotionSettings& settings) {
		if (!settings.hasMovement) {
			return;
		}
		const bool expanded = EditorProperties::Header("Ping Pong","最初に動き始めた位置から終点へ進み、戻ります。変位は親の座標系、一往復の時間は秒で指定します。");
		if (ImGui::BeginPopupContextItem("MovementComponentMenu")) {
			if (ImGui::MenuItem("Componentを削除")) {
				settings.hasMovement = false;
			}
			ImGui::EndPopup();
		}
		if (!settings.hasMovement) {
			return;
		}
		if (!expanded) {
			return;
		}
		if (!EditorProperties::Begin("MovementFields")) {
			return;
		}
		EditorProperties::Toggle("Enabled", &settings.movementEnabled);
		EditorProperties::Vector("Offset",&settings.movementOffset,0.05f,-100000,100000);
		EditorProperties::Float("Period (s)",&settings.movementPeriod,0.05f,0.01f,3600);
		EditorProperties::End();
	}
}
#endif

// 常設ボタンを増やさず、Inspectorの空白を右クリックして追加する。
void InspectorPanel::DrawMotionComponents(const EditableObjectView& object, IEditableScene* scene) {
#ifdef USE_IMGUI
	if (object.motion == nullptr) {
		return;
	}
	if (ImGui::BeginPopupContextWindow("AddMotionComponent", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
		if (ImGui::BeginMenu("Componentを追加")) {
			if (ImGui::MenuItem("Rotation / 自動回転", nullptr, false, !object.motion->hasRotation)) {
				object.motion->hasRotation = true;
			}
			if (ImGui::MenuItem("Ping Pong / 往復移動", nullptr, false, !object.motion->hasMovement)) {
				object.motion->hasMovement = true;
			}
			size_t colliderCount = 0;
			if (scene != nullptr) {
				for (size_t objectIndex = 0; objectIndex < scene->GetEditableObjectCount(); ++objectIndex) {
					const SceneObjectData* data = scene->GetObjectData(objectIndex);
					if (data != nullptr) {
						if (data->collider.present) {
							++colliderCount;
						}
					}
				}
			}
			if (object.collider != nullptr) {
				ImGui::BeginDisabled(object.collider->present || colliderCount >= SceneCollision::kMaxColliders);
				if (ImGui::MenuItem("Box Collider")) {
					object.collider->present = true;
					object.collider->shape = ColliderShape::Box;
				}
				if (ImGui::MenuItem("Sphere Collider")) {
					object.collider->present = true;
					object.collider->shape = ColliderShape::Sphere;
				}
				ImGui::EndDisabled();
			}
			if (object.hasMeshRenderer != nullptr) {
				if (object.type == SceneObjectType::Model || object.type == SceneObjectType::Empty) {
					if (ImGui::MenuItem("Mesh Renderer",nullptr,false,!*object.hasMeshRenderer)) {
						*object.hasMeshRenderer = true;
					}
				}
			}
			if (object.body != nullptr) {
				if (ImGui::MenuItem("Rigidbody / 重力・押し戻し",nullptr,false,!object.body->present)) {
					object.body->present = true;
				}
			}
			ImGui::EndMenu();
		}
		ImGui::EndPopup();
	}

	DrawRotation(*object.motion);
	DrawMovement(*object.motion);
#else
	(void)object;
	(void)scene;
#endif
}
