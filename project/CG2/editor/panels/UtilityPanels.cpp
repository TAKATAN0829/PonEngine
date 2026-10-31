#include "FrameProfiler.h"
#include "UtilityPanels.h"
#include "PrefabFile.h"
#include "../EditorSelection.h"
#include "../AssetDragPayload.h"
#include "LogSystem.h"
#include "SceneObjects.h"
#include <algorithm>
#include <cctype>
#include <limits>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "TextureManager.h"
#endif

std::string UtilityPanels::PathText(const std::filesystem::path& path) {
	const std::u8string text = path.generic_u8string();
	return std::string(text.begin(), text.end());
}

bool UtilityPanels::CompareEntries(const AssetEntry& first, const AssetEntry& second) {
	if (first.directory != second.directory) {
		return first.directory;
	}
	return first.name < second.name;
}

void UtilityPanels::Initialize() {
	std::error_code error;
	root_ = std::filesystem::weakly_canonical("resources", error);
	directory_ = root_;
	initialized_ = true;
	refreshRequested_ = true;
	Update();
}

void UtilityPanels::OpenDirectory(const std::filesystem::path& path) {
	std::error_code error;
	const std::filesystem::path canonical = std::filesystem::weakly_canonical(path, error);
	if (error) {
		status_ = error.message();
		return;
	}
	const std::filesystem::path relative = canonical.lexically_relative(root_);
	if (relative.empty()) {
		return;
	}
	if (*relative.begin() == "..") {
		return;
	}
	directory_ = canonical;
	revealFolder_ = true;
	selected_.clear();
	search_[0] = '\0';
	refreshRequested_ = true;
}

void UtilityPanels::Update() {
	if (!refreshRequested_) {
		return;
	}
	refreshRequested_ = false;
	entries_.clear();
	status_.clear();
	std::error_code error;
	std::filesystem::directory_iterator iterator(directory_, error);
	const std::filesystem::directory_iterator end;
	while (!error && iterator != end) {
		const std::filesystem::directory_entry entry = *iterator;
		if (!entry.is_symlink(error)) {
			AssetEntry asset;
			asset.path = entry.path();
			asset.name = PathText(entry.path().filename());
			asset.extension = PathText(entry.path().extension());
			for (char& character : asset.extension) {
				character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
			}
			asset.directory = entry.is_directory(error);
			entries_.push_back(std::move(asset));
		}
		iterator.increment(error);
	}
	if (error) {
		status_ = error.message();
	}
	std::sort(entries_.begin(), entries_.end(), CompareEntries);
	folders_.clear();
	CacheFolders(root_,0);
}

UtilityPanels::Preview UtilityPanels::GetPreview(const AssetEntry& entry, IEditableScene& scene) {
#ifdef USE_IMGUI
	const std::string key = PathText(entry.path);
	if (previews_.contains(key)) {
		return previews_.at(key);
	}
	if (entry.directory) {
		return {};
	}
	const std::string model = PathText(entry.path.lexically_relative(root_));
	if (SceneObjects::IsSupportedModel(model)) {
		if (previewLoadedThisFrame_) {
			return {};
		}
		previewLoadedThisFrame_ = true;
		Preview modelPreview;
		modelPreview.handle = scene.GetModelPreview(model,status_);
		modelPreview.width = 128;
		modelPreview.height = 128;
		previews_[key] = modelPreview;
		return modelPreview;
	}
	if (entry.extension != ".png" && entry.extension != ".jpg" && entry.extension != ".jpeg" && entry.extension != ".bmp") {
		return {};
	}
	if (previewLoadedThisFrame_) {
		return {};
	}
	if (previews_.size() >= 64) {
		return {};
	}
	previewLoadedThisFrame_ = true;
	Preview preview;
	if (TextureManager::GetInstance()->TryLoad(key, status_)) {
		preview.handle = TextureManager::GetInstance()->GetSrvHandleGPU(key).ptr;
		preview.width = static_cast<float>(TextureManager::GetInstance()->GetMetaData(key).width);
		preview.height = static_cast<float>(TextureManager::GetInstance()->GetMetaData(key).height);
	}
	previews_[key] = preview;
	return preview;
#else
	(void)entry;
	(void)scene;
	return {};
#endif
}

void UtilityPanels::DrawAssetActions(const AssetEntry& entry, IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
#ifdef USE_IMGUI
	if (entry.directory) {
		if (ImGui::MenuItem("開く")) {
			OpenDirectory(entry.path);
		}
		return;
	}
	const std::string relative = PathText(entry.path.lexically_relative(root_));
	if (entry.extension == ".ponprefab") {
		if (ImGui::MenuItem("Prefabをシーンに配置")) {
			scene.PlacePrefab(relative, 0, {}, status_);
		}
		return;
	}
	if (SceneObjects::IsSupportedModel(relative)) {
		if (ImGui::MenuItem("シーンに配置")) {
			if (scene.PrepareModel(relative, status_)) {
				scene.AddObject(relative);
			}
		}
		if (ImGui::MenuItem("選択中のMesh Rendererへ適用")) {
			for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
				const EditableObjectView object = scene.GetEditableObject(objectIndex);
				if (object.id == selection.Get()) {
					if (object.type == SceneObjectType::Model || object.type == SceneObjectType::Empty) {
						if (scene.PrepareModel(relative, status_)) {
							*object.model = relative;
							*object.hasMeshRenderer = true;
							if (object.materials != nullptr) {
								object.materials->clear();
							}
							if (object.animation != nullptr) {
								*object.animation = {};
							}
						}
					}
					break;
				}
			}
		}
	} else if (entry.extension == ".ponparticle") {
		if (ImGui::MenuItem("Particle Editorを開く")) {
			visibility.particleEditor = true;
		}
	} else {
		ImGui::TextDisabled("プレビュー・ファイル情報の表示");
	}
#else
	(void)entry; (void)scene; (void)selection; (void)visibility;
#endif
}

void UtilityPanels::DrawAsset(const AssetEntry& entry, IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
#ifdef USE_IMGUI
	ImGui::PushID(entry.name.c_str());
	ImGui::BeginGroup();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImGui::Selectable("##Asset", selected_ == entry.path, ImGuiSelectableFlags_AllowDoubleClick, {92, 94});
	if (ImGui::IsItemClicked()) {
		selected_ = entry.path;
	}
	if (!entry.directory) {
		const std::string model = PathText(entry.path.lexically_relative(root_));
		const char* payloadType = nullptr;
		if (SceneObjects::IsSupportedModel(model)) {
			payloadType = ModelAssetPayload::kType;
		} else if (entry.extension == ".ponprefab") {
			payloadType = ModelAssetPayload::kPrefabType;
		} else if (entry.extension == ".png" || entry.extension == ".jpg" || entry.extension == ".jpeg" || entry.extension == ".bmp") {
			payloadType = ModelAssetPayload::kTextureType;
		}
		if (payloadType != nullptr && model.size() < sizeof(ModelAssetPayload::model)) {
			if (ImGui::BeginDragDropSource()) {
				ModelAssetPayload payload;
				std::memcpy(payload.model, model.c_str(), model.size() + 1);
				ImGui::SetDragDropPayload(payloadType, &payload, sizeof(payload));
				ImGui::Text("配置: %s", entry.name.c_str());
				ImGui::EndDragDropSource();
			}
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", entry.name.c_str());
		if (entry.directory) {
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				OpenDirectory(entry.path);
			}
		}
	}
	if (ImGui::BeginPopupContextItem("AssetContext")) {
		selected_ = entry.path;
		DrawAssetActions(entry, scene, selection, visibility);
		ImGui::EndPopup();
	}
	if (!ImGui::IsRectVisible(origin, {origin.x + 92, origin.y + 94})) {
		ImGui::EndGroup();
		ImGui::PopID();
		return;
	}
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const Preview preview = GetPreview(entry, scene);
	if (preview.handle != 0) {
		const float scale = (std::min)(64.0f / preview.width, 64.0f / preview.height);
		const ImVec2 size = {preview.width * scale, preview.height * scale};
		const ImVec2 minimum = {origin.x + 46 - size.x * 0.5f, origin.y + 34 - size.y * 0.5f};
		drawList->AddImage(ImTextureRef(static_cast<ImTextureID>(preview.handle)), minimum, {minimum.x + size.x, minimum.y + size.y});
	} else if (entry.directory) {
		drawList->AddRectFilled({origin.x+16,origin.y+17},{origin.x+40,origin.y+25},IM_COL32(210,173,87,255),3);
		drawList->AddRectFilled({origin.x+14,origin.y+23},{origin.x+78,origin.y+61},IM_COL32(224,187,101,255),4);
	} else {
		drawList->AddRect({origin.x+24,origin.y+10},{origin.x+68,origin.y+63},IM_COL32(150,173,197,255),3.0f,2.0f,ImDrawFlags_None);
		const char* typeName = "FILE";
		if (entry.extension == ".obj" || entry.extension == ".gltf") {
			typeName = "3D";
		}
		if (entry.extension == ".ponscene") {
			typeName = "SCENE";
		}
		if (entry.extension == ".ponparticle") {
			typeName = "FX";
		}
		if (entry.extension == ".ponprefab") {
			typeName = "PREFAB";
		}
		if (entry.extension == ".wav") {
			typeName = "AUDIO";
		}
		drawList->AddText({origin.x+28,origin.y+30},IM_COL32(205,215,225,255),typeName);
	}
	drawList->PushClipRect({origin.x+2,origin.y+70},{origin.x+90,origin.y+94},true);
	drawList->AddText({origin.x+3,origin.y+72},ImGui::GetColorU32(ImGuiCol_Text),entry.name.c_str());
	drawList->PopClipRect();
	ImGui::EndGroup();
	ImGui::PopID();
#else
	(void)entry; (void)scene; (void)selection; (void)visibility;
#endif
}

void UtilityPanels::DrawProject(IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
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
			if (ImGui::MenuItem("resourcesへ戻る")) {
				OpenDirectory(root_);
			}
			if (ImGui::MenuItem("上のフォルダへ", nullptr, false, directory_ != root_)) {
				OpenDirectory(directory_.parent_path());
			}
			if (ImGui::MenuItem("更新")) {
				refreshRequested_ = true;
			}
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
			if (ImGui::MenuItem("resourcesへ戻る")) {
				OpenDirectory(root_);
			}
			if (ImGui::MenuItem("上のフォルダへ", nullptr, false, directory_ != root_)) {
				OpenDirectory(directory_.parent_path());
			}
			if (ImGui::MenuItem("更新")) {
				refreshRequested_ = true;
			}
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

void UtilityPanels::DrawConsole() {
#ifdef USE_IMGUI
	if (ImGui::Begin("Console")) {
		ImGui::SetNextItemWidth((std::min)(250.0f,ImGui::GetContentRegionAvail().x*0.6f));
		ImGui::InputTextWithHint("##LogSearch","ログを検索",consoleSearch_,sizeof(consoleSearch_));
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
				if (consoleSearch_[0] != '\0') {
					if (message.find(consoleSearch_) == std::string::npos) {
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

void UtilityPanels::Draw(IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Project);
	if (!initialized_) {
		Initialize();
	}
	Update();
	previewLoadedThisFrame_ = false;
	DrawProject(scene, selection, visibility);
	DrawConsole();
}

// ツリーの構築は明示更新時だけ行い、深さと件数を制限する。シンボリックリンクは辿らない。
void UtilityPanels::CacheFolders(const std::filesystem::path& directory, size_t depth) {
	if (folders_.size() >= 512 || depth > 12) {
		return;
	}
	const size_t folderIndex = folders_.size();
	folders_.push_back({directory,depth,false});
	std::error_code error;
	std::vector<std::filesystem::path> children;
	std::filesystem::directory_iterator iterator(directory,error);
	const std::filesystem::directory_iterator end;
	while (!error && iterator != end) {
		if (!iterator->is_symlink(error)) {
			if (iterator->is_directory(error)) {
				children.push_back(iterator->path());
			}
		}
		iterator.increment(error);
	}
	std::sort(children.begin(),children.end());
	folders_[folderIndex].children = !children.empty() && depth < 12;
	for (const std::filesystem::path& child : children) {
		CacheFolders(child,depth+1);
		if (folders_.size() >= 512) {
			break;
		}
	}
}

void UtilityPanels::DrawFolders() {
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
