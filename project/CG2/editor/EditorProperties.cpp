#include "EditorProperties.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <algorithm>
#include <cfloat>
#endif

bool EditorProperties::Header(const char* title, const char* help) {
#ifdef USE_IMGUI
	ImGui::Spacing();
	const bool expanded = ImGui::CollapsingHeader(title,ImGuiTreeNodeFlags_DefaultOpen);
	if (help != nullptr) {
		// DrawListへの描画だけを行い、見出しの右クリック対象を変えない。
		const ImVec2 maximum = ImGui::GetItemRectMax();
		const ImVec2 minimum = ImGui::GetItemRectMin();
		ImGui::GetWindowDrawList()->AddText({maximum.x-16,minimum.y+ImGui::GetStyle().FramePadding.y},ImGui::GetColorU32(ImGuiCol_TextDisabled),"?");
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s",help);
		}
	}
	return expanded;
#else
	(void)title;
	(void)help;
	return false;
#endif
}

bool EditorProperties::Begin(const char* identifier) {
#ifdef USE_IMGUI
	ImGui::PushID(identifier);
	if (!ImGui::BeginTable("##Properties",2,ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
		ImGui::PopID();
		return false;
	}
	const float labelWidth = (std::clamp)(ImGui::GetContentRegionAvail().x * 0.30f,75.0f,120.0f);
	ImGui::TableSetupColumn("Property",ImGuiTableColumnFlags_WidthFixed,labelWidth);
	ImGui::TableSetupColumn("Value",ImGuiTableColumnFlags_WidthStretch);
	return true;
#else
	(void)identifier;
	return false;
#endif
}

void EditorProperties::Row(const char* label) {
#ifdef USE_IMGUI
	ImGui::TableNextRow();
	ImGui::TableSetColumnIndex(0);
	ImGui::AlignTextToFramePadding();
	ImGui::TextUnformatted(label);
	ImGui::TableSetColumnIndex(1);
	ImGui::SetNextItemWidth(-FLT_MIN);
#else
	(void)label;
#endif
}

void EditorProperties::End() {
#ifdef USE_IMGUI
	ImGui::EndTable();
	ImGui::PopID();
#endif
}

bool EditorProperties::Float(const char* label, float* value, float speed, float minimum, float maximum, const char* format) {
#ifdef USE_IMGUI
	Row(label);
	ImGui::PushID(label);
	const bool changed = ImGui::DragFloat("##Value",value,speed,minimum,maximum,format,ImGuiSliderFlags_AlwaysClamp);
	ImGui::PopID();
	return changed;
#else
	(void)label; (void)value; (void)speed; (void)minimum; (void)maximum; (void)format;
	return false;
#endif
}

bool EditorProperties::Vector(const char* label, Vector3* value, float speed, float minimum, float maximum) {
#ifdef USE_IMGUI
	Row(label);
	ImGui::PushID(label);
	const float width = (std::max)(16.0f,(ImGui::GetContentRegionAvail().x-3*12-5*4)/3);
	const char* axes[] = {"X","Y","Z"};
	const ImVec4 colors[] = {{0.65f,0.24f,0.22f,1},{0.28f,0.58f,0.26f,1},{0.25f,0.42f,0.70f,1}};
	float* components[] = {&value->x,&value->y,&value->z};
	bool changed = false;
	ImGuiSliderFlags flags = ImGuiSliderFlags_None;
	if (minimum < maximum) {
		flags = ImGuiSliderFlags_AlwaysClamp;
	}
	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		if (axisIndex != 0) {
			ImGui::SameLine(0,4);
		}
		ImGui::PushID(static_cast<int>(axisIndex));
		ImGui::AlignTextToFramePadding();
		ImGui::TextColored(colors[axisIndex],"%s",axes[axisIndex]);
		ImGui::SameLine(0,2);
		ImGui::SetNextItemWidth(width);
		changed |= ImGui::DragFloat("##Axis",components[axisIndex],speed,minimum,maximum,"%.2f",flags);
		ImGui::PopID();
	}
	ImGui::PopID();
	return changed;
#else
	(void)label; (void)value; (void)speed; (void)minimum; (void)maximum;
	return false;
#endif
}

bool EditorProperties::Toggle(const char* label, bool* value) {
#ifdef USE_IMGUI
	Row(label);
	ImGui::PushID(label);
	const bool changed = ImGui::Checkbox("##Value",value);
	ImGui::PopID();
	return changed;
#else
	(void)label; (void)value;
	return false;
#endif
}

bool EditorProperties::Combo(const char* label, int* value, const char* choices) {
#ifdef USE_IMGUI
	Row(label);
	ImGui::PushID(label);
	const bool changed = ImGui::Combo("##Value",value,choices);
	ImGui::PopID();
	return changed;
#else
	(void)label; (void)value; (void)choices;
	return false;
#endif
}

bool EditorProperties::Color(const char* label, Vector4* value) {
#ifdef USE_IMGUI
	Row(label);
	ImGui::PushID(label);
	const bool changed = ImGui::ColorEdit4("##Value",&value->x);
	ImGui::PopID();
	return changed;
#else
	(void)label; (void)value;
	return false;
#endif
}
