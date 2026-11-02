#include "ConsolePanel.h"
#include "LogSystem.h"
#include <algorithm>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// ログはLogSystemから借用し、このパネルは表示用の検索条件だけを所有する。
void ConsolePanel::Draw() {
#ifdef USE_IMGUI
	if (ImGui::Begin("Console")) {
		ImGui::SetNextItemWidth((std::min)(250.0f,ImGui::GetContentRegionAvail().x*0.6f));
		ImGui::InputTextWithHint("##LogSearch","ログを検索",search_,sizeof(search_));
		ImGui::SameLine();
		ImGui::TextDisabled("%zu messages",LogSystem::GetRecentMessages().size());
		ImGui::Separator();
		ImGui::BeginChild("ConsoleMessages");
		if (ImGui::BeginTable("LogRows",2,ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
			ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,38);
			ImGui::TableSetupColumn("Message",ImGuiTableColumnFlags_WidthStretch);
			size_t messageIndex = 0;
			for (const std::string& message : LogSystem::GetRecentMessages()) {
				++messageIndex;
				if (search_[0] != '\0') {
					if (message.find(search_) == std::string::npos) {
						continue;
					}
				}
				ImGui::PushID(static_cast<int>(messageIndex));
				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				ImGui::TextDisabled("%zu",messageIndex);
				ImGui::TableSetColumnIndex(1);
				size_t lineEnd = message.find('\n');
				if (lineEnd == std::string::npos) {
					lineEnd = message.size();
				}
				ImGui::TextUnformatted(message.data(),message.data()+lineEnd);
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("%s",message.c_str());
				}
				if (ImGui::BeginPopupContextItem("LogActions")) {
					if (ImGui::MenuItem("全文をコピー")) {
						ImGui::SetClipboardText(message.c_str());
					}
					ImGui::EndPopup();
				}
				ImGui::PopID();
			}
			ImGui::EndTable();
		}
		ImGui::EndChild();
	}
	ImGui::End();
#endif
}
