#include "ConsolePanel.h"
#include "LogSystem.h"
#include <algorithm>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// 描画処理


// ログはLogSystemから借用し、このパネルは表示用の検索条件だけを所有する。
void ConsolePanel::Draw() {
	// ログの検索条件を受け取り、一致したメッセージを表示する。
#ifdef USE_IMGUI
	Window window(*this);
	if (window.IsVisible()) {
		// 検索欄がパネル幅を占めすぎないようにして、右に全件数を表示する。
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth((std::min)(250.0f,ImGui::GetContentRegionAvail().x*0.6f));
		// ヒント付きで文字列を入力。
		ImGui::InputTextWithHint("##LogSearch","ログを検索",search_,sizeof(search_));
		// 横並びに配置。
		ImGui::SameLine();
		// 補助テキストを表示。
		ImGui::TextDisabled("%zu messages",LogSystem::GetRecentMessages().size());
		// 区切り線を表示。
		ImGui::Separator();

		// 検索欄を固定し、メッセージ一覧だけをスクロールする。
		// 「ConsoleMessages」子領域を開始。
		ImGui::BeginChild("ConsoleMessages");
		// 「LogRows」表を開始。
		if (ImGui::BeginTable("LogRows",2,ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
			// 番号列は固定幅、本文列は残りの幅を使う。
			// 「#」表の列を設定。
			ImGui::TableSetupColumn("#",ImGuiTableColumnFlags_WidthFixed,38);
			// 「Message」表の列を設定。
			ImGui::TableSetupColumn("Message",ImGuiTableColumnFlags_WidthStretch);

			size_t messageIndex = 0;

			for (const std::string& message : LogSystem::GetRecentMessages()) {
				// 絞り込んでも元の一覧での行番号を表示する。
				++messageIndex;

				if (search_[0] != '\0') {
					if (message.find(search_) == std::string::npos) {
						continue;
					}
				}

				// 同じ本文のログでも、右クリックメニューを行ごとに区別する。
				// 項目IDを追加。
				ImGui::PushID(static_cast<int>(messageIndex));
				// 表の次行へ移動。
				ImGui::TableNextRow();
				// 表の列を選択。
				ImGui::TableSetColumnIndex(0);
				// 補助テキストを表示。
				ImGui::TextDisabled("%zu",messageIndex);
				// 表の列を選択。
				ImGui::TableSetColumnIndex(1);

				// 一覧は先頭行だけを描き、全文はツールチップとコピーに使う。
				size_t lineEnd = message.find('\n');

				if (lineEnd == std::string::npos) {
					lineEnd = message.size();
				}

				// テキストを表示。
				ImGui::TextUnformatted(message.data(),message.data()+lineEnd);
				// ホバー状態を確認。
				if (ImGui::IsItemHovered()) {
					// 補足説明を表示。
					ImGui::SetTooltip("%s",message.c_str());
				}

				// 「LogActions」項目メニューを開始。
				if (ImGui::BeginPopupContextItem("LogActions")) {
					// 「全文をコピー」メニュー選択を受け取る。
					if (ImGui::MenuItem("全文をコピー")) {
						// クリップボードへコピー。
						ImGui::SetClipboardText(message.c_str());
					}

					// ポップアップを終了。
					ImGui::EndPopup();
				}

				// 項目IDを戻す。
				ImGui::PopID();
			}

			// 表を終了。
			ImGui::EndTable();
		}

		// 子領域を終了。
		ImGui::EndChild();
	}

#endif
}
