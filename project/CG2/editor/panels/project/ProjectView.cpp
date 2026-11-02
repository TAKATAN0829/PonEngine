#include "ProjectPanel.h"
#include <algorithm>
#include <limits>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

// Projectのパンくず・検索・フォルダツリー・Asset一覧の配置を担当する。
void ProjectPanel::DrawContents(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
#ifdef USE_IMGUI
	if (ImGui::Begin("Project",nullptr,ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		const ImVec2 headerOrigin = ImGui::GetCursorScreenPos();
		const float searchWidth = (std::min)(200.0f,ImGui::GetContentRegionAvail().x*0.4f);
		const float availableWidth = ImGui::GetContentRegionAvail().x;
		std::string relative = PathText(directory_.lexically_relative(root_));
		if (relative == ".") {
			relative.clear();
		}
		ImGui::PushClipRect(headerOrigin,{headerOrigin.x+availableWidth-searchWidth-8,headerOrigin.y+ImGui::GetFrameHeight()},true);
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("resources / %s",relative.c_str());
		ImGui::PopClipRect();
		ImGui::SameLine();
		ImGui::SetCursorScreenPos({headerOrigin.x+availableWidth-searchWidth,headerOrigin.y});
		ImGui::SetNextItemWidth(searchWidth);
		ImGui::InputTextWithHint("##AssetSearch", "Assetsを検索", search_, sizeof(search_));
		if (ImGui::BeginPopupContextWindow("ProjectContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			DrawDirectoryActions();
			ImGui::EndPopup();
		}
		ImGui::Separator();
		const float bodyHeight = (std::max)(1.0f,ImGui::GetContentRegionAvail().y-2*ImGui::GetStyle().CellPadding.y);
		if (!ImGui::BeginTable("ProjectSplit",2,ImGuiTableFlags_Resizable | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp,{0,0})) {
			ImGui::End();
			return;
		}
		ImGui::TableSetupColumn("Folders",ImGuiTableColumnFlags_WidthFixed,170);
		ImGui::TableSetupColumn("Assets",ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableNextRow();
		ImGui::TableSetColumnIndex(0);
		ImGui::BeginChild("FolderTree",{0,bodyHeight});
		DrawFolders();
		ImGui::EndChild();
		ImGui::TableSetColumnIndex(1);
		ImGui::BeginChild("AssetGrid", {0,bodyHeight});
		int column = 0;
		int columns = static_cast<int>(ImGui::GetContentRegionAvail().x / 102);
		if (columns < 1) {
			columns = 1;
		}
		for (const AssetEntry& entry : entries_) {
			if (search_[0] != '\0') {
				if (entry.name.find(search_) == std::string::npos) {
					continue;
				}
			}
			DrawAsset(entry, scene, selection, visibility);
			if (++column < columns) {
				ImGui::SameLine();
			}
			else {
				column = 0;
			}
		}
		if (ImGui::BeginPopupContextWindow("ProjectGridContext", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			DrawDirectoryActions();
			ImGui::EndPopup();
		}
		if (!status_.empty()) {
			ImGui::TextWrapped("%s", status_.c_str());
		}
		ImGui::EndChild();
		ImGui::EndTable();
	}
	ImGui::End();
#else
	(void)scene; (void)selection; (void)visibility;
#endif
}

void ProjectPanel::DrawFolders() {
#ifdef USE_IMGUI
	std::filesystem::path requested;
	size_t collapsedDepth = (std::numeric_limits<size_t>::max)();
	for (const FolderEntry& folder : folders_) {
		if (folder.depth > collapsedDepth) {
			continue;
		}
		collapsedDepth = (std::numeric_limits<size_t>::max)();
		const std::string identifier = PathText(folder.path);
		const std::string name = PathText(folder.path.filename());
		ImGui::PushID(identifier.c_str());
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (folder.depth == 0) {
			flags |= ImGuiTreeNodeFlags_DefaultOpen;
		}
		if (!folder.children) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		}
		if (folder.path == directory_) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		if (revealFolder_) {
			const std::filesystem::path relative = directory_.lexically_relative(folder.path);
			if (!relative.empty()) {
				if (*relative.begin() != "..") {
					ImGui::SetNextItemOpen(true);
				}
			}
		}
		const float indent = static_cast<float>(folder.depth)*12;
		if (indent > 0) {
			ImGui::Indent(indent);
		}
		const bool open = ImGui::TreeNodeEx(name.c_str(),flags);
		if (ImGui::IsItemClicked()) {
			if (!ImGui::IsItemToggledOpen()) {
				requested = folder.path;
			}
		}
		if (indent > 0) {
			ImGui::Unindent(indent);
		}
		if (!open) {
			collapsedDepth = folder.depth;
		}
		ImGui::PopID();
	}
	revealFolder_ = false;
	if (!requested.empty()) {
		OpenDirectory(requested);
	}
#endif
}

// 呼出元のPopup内に同じディレクトリ操作を並べ、配置による処理の差を作らない。
void ProjectPanel::DrawDirectoryActions() {
#ifdef USE_IMGUI
	if (ImGui::MenuItem("resourcesへ戻る")) {
		OpenDirectory(root_);
	}
	if (ImGui::MenuItem("上のフォルダへ", nullptr, false, directory_ != root_)) {
		OpenDirectory(directory_.parent_path());
	}
	if (ImGui::MenuItem("更新")) {
		refreshRequested_ = true;
	}
#endif
}
