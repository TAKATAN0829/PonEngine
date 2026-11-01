#include "ProjectPanel.h"
#include "FrameProfiler.h"
#include <algorithm>
#include <cctype>
#include <utility>

// ディレクトリ一覧の所有・更新を担当する。走査は更新要求があるときだけ行う。
std::string ProjectPanel::PathText(const std::filesystem::path& path) {
	const std::u8string text = path.generic_u8string();
	return std::string(text.begin(), text.end());
}

bool ProjectPanel::CompareEntries(const AssetEntry& first, const AssetEntry& second) {
	if (first.directory != second.directory) {
		return first.directory;
	}
	return first.name < second.name;
}

void ProjectPanel::Initialize() {
	std::error_code error;
	root_ = std::filesystem::weakly_canonical("resources", error);
	directory_ = root_;
	initialized_ = true;
	refreshRequested_ = true;
	Update();
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

void ProjectPanel::Update() {
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

// Projectの描画単位で更新とプレビューの読込上限を管理する。
void ProjectPanel::Draw(IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Project);
	if (!initialized_) {
		Initialize();
	}
	Update();
	previewLoadedThisFrame_ = false;
	DrawContents(scene,selection,visibility);
}
