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
	// resourcesを起点に、フォルダとAssetの初回走査を準備する。
	std::error_code error;

	root_ = std::filesystem::weakly_canonical("resources", error);

	directory_ = root_;

	fileActions_.Initialize(root_);

	isInitialized_ = true;

	isRefreshRequested_ = true;

	Update();
}

//=============================================================================================//
// 更新処理 / 一覧のキャッシュ

void ProjectPanel::Update() {
	// 更新要求がある場合だけ、現在のフォルダ内容を読み直す。
	if (!isRefreshRequested_) {
		return;
	}

	isRefreshRequested_ = false;

	entries_.clear();


	std::error_code error;

	std::filesystem::directory_iterator iterator(directory_, error);

	const std::filesystem::directory_iterator end;

	while (!error && iterator != end) {
		const std::filesystem::directory_entry entry = *iterator;

		// 復旧用のゴミ箱は通常の一覧に表示しない。
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

			asset.isDirectory = entry.is_directory(error);

			entries_.push_back(std::move(asset));
		}

		iterator.increment(error);
	}

	if (error) {
		status_ = error.message();
	}

	// フォルダを先にし、名前の順で表示する。
	std::sort(entries_.begin(), entries_.end(), ShouldSortEntryBefore);

	folders_.clear();

	// 左側のフォルダツリーも作り直す。
	CacheFolders(root_,0);
}

//=============================================================================================//
// 描画処理

// Projectの描画単位で更新とプレビューの読込上限を管理する。
void ProjectPanel::Draw(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility,
	EditorSession& history, ParticleEditorPanel& particles, const std::filesystem::path& activeScenePath) {
	// 一覧の更新と読込上限を管理し、ファイル操作の結果を受け取る。
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Project);

	if (!isInitialized_) {
		Initialize();
	}

	Update();

	// モデルのプレビュー読込回数をフレーム単位で制限する。
	wasPreviewLoadedThisFrame_ = false;

	areFileOperationsAllowed_ = !history.IsPlaying();

	DrawContents(scene, selection, visibility, history, particles, activeScenePath);

	AssetPathChange change;

	// Asset移動後も、現在のフォルダと選択先を追従させる。
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

		isRefreshRequested_ = true;

		std::error_code pathError;

		if (!std::filesystem::is_directory(directory_, pathError)) {
			OpenDirectory(root_);
		}
	}
}

//=============================================================================================//
// 操作要求

bool ProjectPanel::ConsumeOpenScene(std::filesystem::path& path) {
	// 開くSceneの要求を取り出し、保留中のパスを消す。
	if (requestedScene_.empty()) {
		return false;
	}

	path = requestedScene_;

	requestedScene_.clear();

	return true;
}

bool ProjectPanel::ConsumeAssetPathChange(AssetPathChange& change) {
	// 未処理のAsset移動通知を取り出し、二重通知を防ぐ。
	if (!changedAssetPath_.has_value()) {
		return false;
	}

	change = *changedAssetPath_;

	changedAssetPath_.reset();

	return true;
}

void ProjectPanel::RequestRefresh() {
	// 次のProject更新で一覧を読み直すよう要求する。
	isRefreshRequested_ = true;
}

void ProjectPanel::RevealAsset(const std::filesystem::path& path) {
	// Assetのあるフォルダへ移動し、一覧の選択を合わせる。
	if (!isInitialized_) {
		Initialize();
	}

	OpenDirectory(path.parent_path());

	selected_ = path;

	isRefreshRequested_ = true;
}

//=============================================================================================//
// ディレクトリ管理

bool ProjectPanel::ShouldSortEntryBefore(const AssetEntry& first, const AssetEntry& second) {
	// フォルダを優先し、同じ種類は名前の順に並べる。
	if (first.isDirectory != second.isDirectory) {
		return first.isDirectory;
	}

	return first.name < second.name;
}

// ディレクトリ一覧の所有・更新を担当する。走査は更新要求があるときだけ行う。
std::string ProjectPanel::PathText(const std::filesystem::path& path) {
	// ファイルパスをUIへ表示するUTF-8の文字列へ変換する。
	const std::u8string text = path.generic_u8string();

	return std::string(text.begin(), text.end());
}

void ProjectPanel::OpenDirectory(const std::filesystem::path& path) {
	// 選択先がresources内のフォルダか確認し、一覧の更新を要求する。
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

	shouldRevealFolder_ = true;

	selected_.clear();

	search_[0] = '\0';

	isRefreshRequested_ = true;
}

void ProjectPanel::CacheFolders(const std::filesystem::path& directory, size_t depth) {
	// 深さと走査件数を制限しながら、フォルダ構成を収集する。
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

	folders_[folderIndex].hasChildren = !children.empty() && depth < 12;

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
	// Projectの検索・フォルダツリー・Asset一覧を配置する。
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
	// フォルダツリーを描き、移動・選択・右クリックを処理する。
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

		if (!folder.hasChildren) {
			flags |= ImGuiTreeNodeFlags_Leaf;
		}

		if (folder.path == directory_) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		if (shouldRevealFolder_) {
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

		const bool isOpen = ImGui::TreeNodeEx(name.c_str(),flags);

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

		fileActions_.AcceptMoveDrop(folder.path, areFileOperationsAllowed_);

		if (ImGui::BeginPopupContextItem("FolderActions")) {
			fileActions_.DrawMenu(folder.path, true, areFileOperationsAllowed_);

			ImGui::EndPopup();
		}


		if (indent > 0) {
			ImGui::Unindent(indent);
		}

		if (!isOpen) {
			collapsedDepth = folder.depth;
		}

		ImGui::PopID();
	}

	shouldRevealFolder_ = false;

	if (!requested.empty()) {
		OpenDirectory(requested);
	}

#endif
}

// 呼出元のPopup内に同じディレクトリ操作を並べ、配置による処理の差を作らない。
void ProjectPanel::DrawDirectoryActions() {
	// フォルダ上の右クリックから作成・移動などを要求する。
#ifdef USE_IMGUI
	fileActions_.DrawCreateMenu(directory_, areFileOperationsAllowed_);

	if (ImGui::MenuItem("resourcesへ戻る")) {
		OpenDirectory(root_);
	}

	if (ImGui::MenuItem("上のフォルダへ", nullptr, false, directory_ != root_)) {
		OpenDirectory(directory_.parent_path());
	}

	if (ImGui::MenuItem("更新")) {
		isRefreshRequested_ = true;
	}

#endif
}

//=============================================================================================//
// Asset操作・プレビュー

void ProjectPanel::DrawAsset(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
	// Assetのプレビューと名前を描き、選択やドロップを処理する。
#ifdef USE_IMGUI
	ImGui::PushID(entry.name.c_str());

	ImGui::BeginGroup();

	const ImVec2 origin = ImGui::GetCursorScreenPos();

	ImGui::Selectable("##Asset", selected_ == entry.path, ImGuiSelectableFlags_AllowDoubleClick, {92, 94});

	if (ImGui::IsItemClicked()) {
		selected_ = entry.path;

		if (areFileOperationsAllowed_) {
			if (entry.extension == ".ponscene") {
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					requestedScene_ = entry.path;
				}
			}
		}
	}

	if (entry.isDirectory) {
		fileActions_.AcceptMoveDrop(entry.path, areFileOperationsAllowed_);
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

		if (entry.isDirectory) {
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
	} else if (entry.isDirectory) {
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
	// Assetの種類に応じた配置・適用・読込のメニューを表示する。
#ifdef USE_IMGUI
	fileActions_.DrawMenu(entry.path, entry.isDirectory, areFileOperationsAllowed_);

	if (entry.isDirectory) {
		if (ImGui::MenuItem("開く")) {
			OpenDirectory(entry.path);
		}

		return;
	}

	const std::string relative = PathText(entry.path.lexically_relative(root_));

	if (entry.extension == ".ponscene") {
		if (ImGui::MenuItem("Sceneを開く", nullptr, false, areFileOperationsAllowed_)) {
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

				if (object.id == selection.GetPrimarySelectedObjectId()) {
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
			visibility.isParticleEditorVisible = true;
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
	// Assetの種類に応じた画像を、キャッシュと読込上限を使って取得する。
#ifdef USE_IMGUI
	const std::string key = PathText(entry.path);

	if (previews_.contains(key)) {
		return previews_.at(key);
	}

	// フォルダは画像プレビューの対象にしない。
	if (entry.isDirectory) {
		return {};
	}

	const std::string model = PathText(entry.path.lexically_relative(root_));

	if (WorldObjects::IsSupportedModel(model)) {
		if (wasPreviewLoadedThisFrame_) {
			return {};
		}

		wasPreviewLoadedThisFrame_ = true;

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

	if (wasPreviewLoadedThisFrame_) {
		return {};
	}

	if (previews_.size() >= 64) {
		return {};
	}

	wasPreviewLoadedThisFrame_ = true;

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
