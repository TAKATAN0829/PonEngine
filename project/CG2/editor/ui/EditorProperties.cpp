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
	// 内部で使う表名が同じでも、セクションごとに識別できるようにする。
	ImGui::PushID(identifier);

	if (!ImGui::BeginTable("##Properties",2,ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoSavedSettings)) {
		ImGui::PopID();

		return false;
	}

	// ラベル列は表示幅の3割を基準に75〜120ピクセルへ収め、残りを入力欄に使う。
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

	// 次の入力欄は、値の列の右端近くまで広げる。
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
	(void)label; 
	(void)value;
	(void)speed; 
	(void)minimum; 
	(void)maximum;
	(void)format;

	return false;

#endif
}

bool EditorProperties::Vector(const char* label, Vector3* value, float speed, float minimum, float maximum) {
	// 三成分の編集欄を並べ、いずれかが変わったか返す。
#ifdef USE_IMGUI
	Row(label);

	// 項目名でIDを分け、PositionとScaleなどで同じ軸の入力欄が衝突しないようにする。
	ImGui::PushID(label);

	// 軸ラベルと余白の見込み分を引いて3等分し、各入力欄は最低16ピクセル確保する。
	const float width = (std::max)(16.0f,(ImGui::GetContentRegionAvail().x-3*12-5*4)/3);

	// 入力欄の横に表示する軸名。以下の配列もX、Y、Zの順に揃える。
	const char* axes[] = {"X","Y","Z"};

	// 軸ラベルの色はXが赤、Yが緑、Zが青。
	const ImVec4 colors[] = {{0.65f,0.24f,0.22f,1},{0.28f,0.58f,0.26f,1},{0.25f,0.42f,0.70f,1}};

	// DragFloatが元のVector3の各成分へ直接書き込めるよう、アドレスを渡す。
	float* components[] = {&value->x,&value->y,&value->z};

	// どれか1軸でも編集されたらtrueにする。
	bool hasChanged = false;

	// 最小値と最大値で範囲が指定された場合だけ、その範囲内に制限する。
	ImGuiSliderFlags flags = ImGuiSliderFlags_None;

	if (minimum < maximum) {
		flags = ImGuiSliderFlags_AlwaysClamp;
	}

	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		if (axisIndex != 0) {
			ImGui::SameLine(0,4);
		}

		// 項目内でも軸番号でIDを分け、各軸を個別に操作できるようにする。
		ImGui::PushID(static_cast<int>(axisIndex));

		ImGui::AlignTextToFramePadding();

		ImGui::TextColored(colors[axisIndex],"%s",axes[axisIndex]);

		ImGui::SameLine(0,2);

		ImGui::SetNextItemWidth(width);

		// この軸の変更結果を、ここまでの変更結果に足し合わせる。
		hasChanged |= ImGui::DragFloat("##Axis",components[axisIndex],speed,minimum,maximum,"%.2f",flags);

		ImGui::PopID();
	}

	ImGui::PopID();

	return hasChanged;

#else
	(void)label;
	(void)value; 
	(void)speed; 
	(void)minimum;
	(void)maximum;

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
	(void)label; 
	(void)isChecked;

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
	(void)label;
	(void)value;
	(void)choices;

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
	(void)label; 
	(void)value;

	return false;

#endif
}
