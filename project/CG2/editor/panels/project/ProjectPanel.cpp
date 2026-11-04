#include "ProjectPanel.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <cctype>
#include <utility>
#include <limits>
#include "../../EditorSelection.h"
#include "../../AssetDragPayload.h"
#include "objects/WorldObjects.h"
#include <cstring>
#include "TextureManager.h"
#include "LogSystem.h"
#include "../../EditorSession.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// 初期化処理


void ProjectPanel::Initialize() {
	std::error_code error;

	root_ = std::filesystem::weakly_canonical("resources", error);

	directory_ = root_;

	fileActions_.Initialize(root_);

	initialized_ = true;

	refreshRequested_ = true;

	Update();
}

//=============================================================================================//
// 更新処理 / 一覧のキャッシュ


void ProjectPanel::Update() {
	if (!refreshRequested_) {
		return;
	}

	refreshRequested_ = false;

	entries_.clear();


	std::error_code error;

	std::filesystem::directory_iterator iterator(directory_, error);

	const std::filesystem::directory_iterator end;

	while (!error && iterator != end) {
		const std::filesystem::directory_entry entry = *iterator;

		if (entry.path().filename() == ".editor-trash") {
			iterator.increment(error);

			continue;
		}

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

//=============================================================================================//
// 描画処理


// Projectの描画単位で更新とプレビューの読込上限を管理する。
void ProjectPanel::Draw(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility,
	EditorSession& history, ParticleEditorPanel& particles, const std::filesystem::path& activeScenePath) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Project);

	if (!initialized_) {
		Initialize();
	}

	Update();

	previewLoadedThisFrame_ = false;

	fileOperationsAllowed_ = !history.IsPlaying();

	DrawContents(scene, selection, visibility, history, particles, activeScenePath);

	AssetPathChange change;

	if (fileActions_.ConsumePathChange(change)) {
		changedAssetPath_ = change;
		directory_ = root_ / AssetPathChange::Path(change.RemapRelative(PathText(directory_.lexically_relative(root_))));

		if (!selected_.empty()) {
			selected_ = root_ / AssetPathChange::Path(change.RemapRelative(PathText(selected_.lexically_relative(root_))));
		}
	}

	if (fileActions_.ConsumeRefresh()) {
		status_ = fileActions_.GetStatus();

		LogSystem::Log(status_ + "\n");

		previews_.clear();

		refreshRequested_ = true;

		std::error_code pathError;

		if (!std::filesystem::is_directory(directory_, pathError)) {
			OpenDirectory(root_);
		}
	}
}

//=============================================================================================//
// 操作要求

bool ProjectPanel::ConsumeOpenScene(std::filesystem::path& path) {
	if (requestedScene_.empty()) {
		return false;
	}

	path = requestedScene_;

	requestedScene_.clear();

	return true;
}

bool ProjectPanel::ConsumeAssetPathChange(AssetPathChange& change) {
	if (!changedAssetPath_.has_value()) {
		return false;
	}

	change = *changedAssetPath_;

	changedAssetPath_.reset();

	return true;
}

void ProjectPanel::RevealAsset(const std::filesystem::path& path) {
	if (!initialized_) {
		Initialize();
	}

	OpenDirectory(path.parent_path());

	selected_ = path;

	refreshRequested_ = true;
}

//=============================================================================================//
// ディレクトリ管理


bool ProjectPanel::CompareEntries(const AssetEntry& first, const AssetEntry& second) {
	if (first.directory != second.directory) {
		return first.directory;
	}

	return first.name < second.name;
}

// ディレクトリ一覧の所有・更新を担当する。走査は更新要求があるときだけ行う。
std::string ProjectPanel::PathText(const std::filesystem::path& path) {
	const std::u8string text = path.generic_u8string();

	return std::string(text.begin(), text.end());
}

void ProjectPanel::OpenDirectory(const std::filesystem::path& path) {
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

void ProjectPanel::CacheFolders(const std::filesystem::path& directory, size_t depth) {
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
		if (iterator->path().filename() == ".editor-trash") {
			iterator.increment(error);

			continue;
		}

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

//=============================================================================================//
// 画面構成


// Projectのパンくず・検索・フォルダツリー・Asset一覧の配置を担当する。
void ProjectPanel::DrawContents(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility,
	EditorSession& history, ParticleEditorPanel& particles, const std::filesystem::path& activeScenePath) {
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

	fileActions_.Draw(scene, history, particles, activeScenePath);

	ImGui::End();

#else
	(void)scene;

	(void)selection;

	(void)visibility;

	(void)activeScenePath;

	(void)history;

	(void)particles;

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


		if (folder.path != root_) {
			if (ImGui::BeginDragDropSource()) {
				const std::string relative = PathText(folder.path.lexically_relative(root_));

				if (relative.size() < sizeof(ModelAssetPayload::model)) {
					ModelAssetPayload payload;

					std::memcpy(payload.model, relative.c_str(), relative.size() + 1);

					ImGui::SetDragDropPayload(ModelAssetPayload::kProjectType, &payload, sizeof(payload));
				}

				ImGui::TextUnformatted(name.c_str());

				ImGui::EndDragDropSource();
			}
		}

		fileActions_.AcceptMoveDrop(folder.path, fileOperationsAllowed_);

		if (ImGui::BeginPopupContextItem("FolderActions")) {
			fileActions_.DrawMenu(folder.path, true, fileOperationsAllowed_);

			ImGui::EndPopup();
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
	fileActions_.DrawCreateMenu(directory_, fileOperationsAllowed_);

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

//=============================================================================================//
// Asset操作・プレビュー


void ProjectPanel::DrawAsset(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
#ifdef USE_IMGUI
	ImGui::PushID(entry.name.c_str());

	ImGui::BeginGroup();

	const ImVec2 origin = ImGui::GetCursorScreenPos();

	ImGui::Selectable("##Asset", selected_ == entry.path, ImGuiSelectableFlags_AllowDoubleClick, {92, 94});

	if (ImGui::IsItemClicked()) {
		selected_ = entry.path;

		if (fileOperationsAllowed_) {
			if (entry.extension == ".ponscene") {
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					requestedScene_ = entry.path;
				}
			}
		}
	}

	if (entry.directory) {
		fileActions_.AcceptMoveDrop(entry.path, fileOperationsAllowed_);
	}

	{
		const std::string model = PathText(entry.path.lexically_relative(root_));

		const char* payloadType = ModelAssetPayload::kProjectType;

		if (WorldObjects::IsSupportedModel(model)) {
			payloadType = ModelAssetPayload::kType;
		} else if (entry.extension == ".ponprefab") {
			payloadType = ModelAssetPayload::kPrefabType;
		} else if (entry.extension == ".png" || entry.extension == ".jpg" || entry.extension == ".jpeg" || entry.extension == ".bmp") {
			payloadType = ModelAssetPayload::kTextureType;
		}

		if (model.size() < sizeof(ModelAssetPayload::model)) {
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

		drawList->AddText({origin.x+28,origin.y+30},IM_COL32(55,35,75,255),typeName);
	}

	drawList->PushClipRect({origin.x+2,origin.y+70},{origin.x+90,origin.y+94},true);

	drawList->AddText({origin.x+3,origin.y+72},ImGui::GetColorU32(ImGuiCol_Text),entry.name.c_str());

	drawList->PopClipRect();

	ImGui::EndGroup();

	ImGui::PopID();

#else
	(void)entry;

	(void)scene;

	(void)selection;

	(void)visibility;

#endif
}

void ProjectPanel::DrawAssetActions(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
#ifdef USE_IMGUI
	fileActions_.DrawMenu(entry.path, entry.directory, fileOperationsAllowed_);

	if (entry.directory) {
		if (ImGui::MenuItem("開く")) {
			OpenDirectory(entry.path);
		}

		return;
	}

	const std::string relative = PathText(entry.path.lexically_relative(root_));

	if (entry.extension == ".ponscene") {
		if (ImGui::MenuItem("Sceneを開く", nullptr, false, fileOperationsAllowed_)) {
			requestedScene_ = entry.path;
		}

		return;
	}

	if (entry.extension == ".ponprefab") {
		if (ImGui::MenuItem("Prefabをシーンに配置")) {
			scene.PlacePrefab(relative, 0, {}, status_);
		}

		return;
	}

	if (WorldObjects::IsSupportedModel(relative)) {
		if (ImGui::MenuItem("シーンに配置")) {
			if (scene.PrepareModel(relative, status_)) {
				scene.AddObject(relative);
			}
		}

		if (ImGui::MenuItem("選択中のMesh Rendererへ適用")) {
			for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
				const EditableObjectView object = scene.GetEditableObject(objectIndex);

				if (object.id == selection.Get()) {
					if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
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
	(void)entry;

	(void)scene;

	(void)selection;

	(void)visibility;

#endif
}

// Assetタイル・サムネイル・右クリック・ドラッグ＆ドロップを担当する。
ProjectPanel::Preview ProjectPanel::GetPreview(const AssetEntry& entry, IEditableWorld& scene) {
#ifdef USE_IMGUI
	const std::string key = PathText(entry.path);

	if (previews_.contains(key)) {
		return previews_.at(key);
	}

	if (entry.directory) {
		return {};
	}

	const std::string model = PathText(entry.path.lexically_relative(root_));

	if (WorldObjects::IsSupportedModel(model)) {
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
