#include "../EditorProperties.h"
#include "InspectorPanel.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void InspectorPanel::DrawBodyComponent(const EditableObjectView& object) {
#ifdef USE_IMGUI
	if (object.body == nullptr) {
		return;
	}
	if (!object.body->present) {
		return;
	}
	const bool expanded = EditorProperties::Header("Rigidbody","Dynamicは重力と接触で移動します。動く床はKinematic、床・壁はStaticまたはRigidbodyなし。Dynamicは動かない親の下に置きます。角速度と連続衝突判定は未対応です。");
	if (ImGui::BeginPopupContextItem("RigidbodyMenu")) {
		if (ImGui::MenuItem("Componentを削除")) {
			object.body->present = false;
		}
		ImGui::EndPopup();
	}
	if (!object.body->present) {
		return;
	}
	if (!expanded) {
		return;
	}
	if (!EditorProperties::Begin("BodyFields")) {
		return;
	}
	EditorProperties::Toggle("Enabled",&object.body->enabled);
	int mode = static_cast<int>(object.body->mode);
	if (EditorProperties::Combo("Mode",&mode,"Static\0Dynamic\0Kinematic\0")) {
		object.body->mode = static_cast<BodyMode>(mode);
	}
	ImGui::BeginDisabled(object.body->mode != BodyMode::Dynamic);
	EditorProperties::Toggle("Use Gravity",&object.body->useGravity);
	EditorProperties::Float("Mass",&object.body->mass,0.1f,0.001f,100000,"%.3f");
	EditorProperties::Float("Gravity Scale",&object.body->gravityScale,0.05f,0,10,"%.2f");
	EditorProperties::Vector("Velocity",&object.body->initialVelocity,0.1f,-1000,1000);
	EditorProperties::Float("Damping",&object.body->linearDamping,0.05f,0,100,"%.2f");
	ImGui::EndDisabled();
	EditorProperties::Float("Bounce",&object.body->restitution,0.01f,0,1);
	EditorProperties::Float("Friction",&object.body->friction,0.01f,0,1);
	EditorProperties::End();
#else
	(void)object;
#endif
}
