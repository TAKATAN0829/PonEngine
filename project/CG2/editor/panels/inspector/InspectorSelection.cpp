#include "../../Editor.h"
#include "../../EditorProperties.h"
#include "InspectorPanel.h"
#include "FrameProfiler.h"
#include <unordered_set>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void Editor::DrawInspector(IEditableWorld& scene) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Inspector);
	selection_.Prune(scene);
	if (selection_.GetIds().size() > 1) {
		DrawMultipleInspector(scene);
		return;
	}
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == kInvalidWorldObjectId) {
			continue;
		}
		if (object.id == selection_.Get()) {
			InspectorPanel::Draw(object, &toolVisibility_.particleEditor, &scene);
			return;
		}
	}
	selection_.Clear();
	InspectorPanel::Draw({});
}

// 複数選択では共通項目を表示し、Transformは主選択との差分で編集する。
void Editor::DrawMultipleInspector(IEditableWorld& scene) {
#ifdef USE_IMGUI
	if (!ImGui::Begin("Inspector")) {
		ImGui::End();
		return;
	}
	ImGui::Text("選択: %zu objects", selection_.GetIds().size());
	ImGui::TextDisabled("主選択を基準に一括編集");
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Transformはローカル差分を選択ルートへ、色とActiveは選択全体へ適用します。");
	}
	EditableObjectView primary;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		if (scene.GetEditableObject(objectIndex).id == selection_.Get()) {
			primary = scene.GetEditableObject(objectIndex);
			break;
		}
	}
	if (primary.transform == nullptr) {
		ImGui::End();
		return;
	}
	if (primary.active == nullptr || primary.color == nullptr) {
		ImGui::End();
		return;
	}
	const TransformData original = *primary.transform;
	TransformData edited = original;
	bool transformChanged = false;
	bool active = *primary.active;
	Vector4 color = *primary.color;
	bool activeChanged = false;
	bool colorChanged = false;
	if (EditorProperties::Begin("MultipleFields")) {
		transformChanged = EditorProperties::Vector("Position",&edited.translate,0.05f);
		Vector3 degrees = MathUtility::Multiply(57.2957795f,edited.rotate);
		if (EditorProperties::Vector("Rotation",&degrees,0.5f)) {
			edited.rotate = MathUtility::Multiply(0.0174532925f,degrees);
			transformChanged = true;
		}
		transformChanged |= EditorProperties::Vector("Scale",&edited.scale,0.01f);
		activeChanged = EditorProperties::Toggle("Active",&active);
		colorChanged = EditorProperties::Color("Color",&color);
		EditorProperties::End();
	}
	if (transformChanged || activeChanged || colorChanged) {
		std::unordered_set<WorldObjectId> roots;
		for (WorldObjectId identifier : selection_.GetRoots(scene)) {
			roots.insert(identifier);
		}
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);
			if (!selection_.Contains(object.id)) {
				continue;
			}
			if (transformChanged) {
				if (roots.contains(object.id)) {
					object.transform->translate = MathUtility::Add(object.transform->translate, MathUtility::Subtract(edited.translate, original.translate));
					object.transform->rotate = MathUtility::Add(object.transform->rotate, MathUtility::Subtract(edited.rotate, original.rotate));
					object.transform->scale = MathUtility::Add(object.transform->scale, MathUtility::Subtract(edited.scale, original.scale));
				}
			}
			if (activeChanged) {
				*object.active = active;
			}
			if (colorChanged) {
				*object.color = color;
			}
		}
	}
	ImGui::End();
#else
	(void)scene;
#endif
}
