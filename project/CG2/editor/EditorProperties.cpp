#include "EditorProperties.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <algorithm>
#include <cfloat>
#endif

//=============================================================================================//
// 描画処理 / セクション・プロパティ表


bool EditorProperties::Header(const char* title, const char* help) {
#ifdef USE_IMGUI
	ImGui::Spacing();

	const bool isExpanded = ImGui::CollapsingHeader(title,ImGuiTreeNodeFlags_DefaultOpen);

	if (help != nullptr) {
		// DrawListへの描画だけを行い、見出しの右クリック対象を変えない。
		const ImVec2 maximum = ImGui::GetItemRectMax();
		const ImVec2 minimum = ImGui::GetItemRectMin();

		ImGui::GetWindowDrawList()->AddText({maximum.x-16,minimum.y+ImGui::GetStyle().FramePadding.y},ImGui::GetColorU32(ImGuiCol_TextDisabled),"?");

		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s",help);
		}
	}

	return isExpanded;

#else
	// 使わんので捨てる
	(void)title;

	(void)help;

	return false;

#endif
}

bool EditorProperties::Begin(const char* identifier) {
	// プロパティを名前と編集欄の二列で表示する準備をする。
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
	// プロパティ名と編集欄を別の列へ配置する。
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
	// 開始したプロパティの表とスタイル設定を終了する。
#ifdef USE_IMGUI
	ImGui::EndTable();

	ImGui::PopID();

#endif
}

//=============================================================================================//
// 描画処理 / 値の編集。戻り値は変更の有無。


bool EditorProperties::Float(const char* label, float* value, float speed, float minimum, float maximum, const char* format) {
	// 数値の編集欄へ範囲と刻みを渡し、変更の有無を返す。
#ifdef USE_IMGUI
	Row(label);

	ImGui::PushID(label);

	const bool hasChanged = ImGui::DragFloat("##Value",value,speed,minimum,maximum,format,ImGuiSliderFlags_AlwaysClamp);

	ImGui::PopID();

	return hasChanged;

#else
	(void)label; (void)value; (void)speed; (void)minimum; (void)maximum; (void)format;

	return false;

#endif
}

bool EditorProperties::Vector(const char* label, Vector3* value, float speed, float minimum, float maximum) {
	// 三成分の編集欄を並べ、いずれかが変わったか返す。
#ifdef USE_IMGUI
	Row(label);

	ImGui::PushID(label);

	const float width = (std::max)(16.0f,(ImGui::GetContentRegionAvail().x-3*12-5*4)/3);

	const char* axes[] = {"X","Y","Z"};

	const ImVec4 colors[] = {{0.65f,0.24f,0.22f,1},{0.28f,0.58f,0.26f,1},{0.25f,0.42f,0.70f,1}};

	float* components[] = {&value->x,&value->y,&value->z};

	bool hasChanged = false;

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

		hasChanged |= ImGui::DragFloat("##Axis",components[axisIndex],speed,minimum,maximum,"%.2f",flags);

		ImGui::PopID();
	}

	ImGui::PopID();

	return hasChanged;

#else
	(void)label; (void)value; (void)speed; (void)minimum; (void)maximum;

	return false;

#endif
}

bool EditorProperties::Toggle(const char* label, bool* isChecked) {
	// UIでbool値を切り替え、変更があったか返す。
#ifdef USE_IMGUI
	Row(label);

	ImGui::PushID(label);

	const bool hasChanged = ImGui::Checkbox("##Value",isChecked);

	ImGui::PopID();

	return hasChanged;

#else
	(void)label; (void)isChecked;

	return false;

#endif
}

bool EditorProperties::Combo(const char* label, int* value, const char* choices) {
	// 候補の選択欄を表示し、選んだ項目を設定へ反映する。
#ifdef USE_IMGUI
	Row(label);

	ImGui::PushID(label);

	const bool hasChanged = ImGui::Combo("##Value",value,choices);

	ImGui::PopID();

	return hasChanged;

#else
	(void)label; (void)value; (void)choices;

	return false;

#endif
}

bool EditorProperties::Color(const char* label, Vector4* value) {
	// 色の編集欄を表示し、変更した値を呼出側へ返す。
#ifdef USE_IMGUI
	Row(label);

	ImGui::PushID(label);

	const bool hasChanged = ImGui::ColorEdit4("##Value",&value->x);

	ImGui::PopID();

	return hasChanged;

#else
	(void)label; (void)value;

	return false;

#endif
}
