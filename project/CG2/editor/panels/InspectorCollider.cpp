#include "../EditorProperties.h"
#include "InspectorPanel.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// BoxとSphereは一つのColliderの形状として切り替える。
void InspectorPanel::DrawColliderComponent(const EditableObjectView& object) {
#ifdef USE_IMGUI
	if (object.collider == nullptr) {
		return;
	}
	if (!object.collider->present) {
		return;
	}
	const bool expanded = EditorProperties::Header("Collider","Center・Size・RadiusはTransformに対するローカル値です。Sphereは非均等拡縮・せん断では外接球を使います。Triggerは通知のみ。Colliderは最大256個です。");
	if (ImGui::BeginPopupContextItem("ColliderMenu")) {
		if (ImGui::MenuItem("Componentを削除")) {
			object.collider->present = false;
		}
		ImGui::EndPopup();
	}
	if (!object.collider->present) {
		return;
	}
	if (!expanded) {
		return;
	}
	if (!EditorProperties::Begin("ColliderFields")) {
		return;
	}
	EditorProperties::Toggle("Enabled",&object.collider->enabled);
	EditorProperties::Toggle("Is Trigger",&object.collider->trigger);
	int shape = static_cast<int>(object.collider->shape);
	if (EditorProperties::Combo("Shape", &shape, "Box\0Sphere\0")) {
		object.collider->shape = static_cast<ColliderShape>(shape);
	}
	EditorProperties::Vector("Center",&object.collider->center,0.05f,-100000,100000);
	if (object.collider->shape == ColliderShape::Box) {
		EditorProperties::Vector("Size",&object.collider->size,0.05f,0.001f,100000);
	} else {
		EditorProperties::Float("Radius",&object.collider->radius,0.05f,0.001f,100000,"%.3f");
	}
	EditorProperties::End();
#else
	(void)object;
#endif
}
