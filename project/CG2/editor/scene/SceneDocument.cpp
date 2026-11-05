#include "SceneDocument.h"
#include "serialization/SceneFile.h"
#include <Windows.h>
#include <cctype>
#include <cwctype>

//=============================================================================================//
// 初期化処理

bool SceneDocument::Initialize(const std::filesystem::path& resources, const std::filesystem::path& initialPath, std::string& error) {
	// resourcesの実体パスを決め、起動時に読込済みの保存先を引き継ぐ。
	path_.clear();

	std::error_code fileError;

	// パス比較の基準となるresourcesの実体を確認する。
	root_ = std::filesystem::canonical(resources, fileError);

	if (fileError) {
		error = "Cannot resolve resources: " + fileError.message();

		return false;
	}

	if (!initialPath.empty()) {
		return ResolvePath(initialPath, true, path_, error);
	}

	error.clear();

	return true;
}

//=============================================================================================//
// Getter / 編集中のファイル

const std::filesystem::path& SceneDocument::GetRoot() const {
	// 操作対象となるresourcesのルートを取得する。
	return root_;
}

const std::filesystem::path& SceneDocument::GetPath() const {
	// 編集中のファイルのパスを取得する。
	return path_;
}

bool SceneDocument::HasPath() const {
	// 編集中のシーンに保存先が設定されているか確認する。
	if (!path_.empty()) {
		return true;
	}

	return false;
}

std::string SceneDocument::GetName() const {
	// 未保存ならUntitled、保存先があれば拡張子を除いた名前を返す。
	if (path_.empty()) {
		return "Untitled";
	}

	return AssetPathChange::Text(path_.stem());
}

//=============================================================================================//
// 新規・読込・保存

bool SceneDocument::New(IEditableWorld& world, std::string& error) {
	// 空の配置への置換に成功してから、以前の保存先を外す。
	if (!world.ReplaceObjects({}, error)) {
		return false;
	}

	path_.clear();

	world.OnSceneContentsChanged();

	return true;
}

bool SceneDocument::Open(IEditableWorld& world, const std::filesystem::path& path, std::string& error) {
	// パスと保存内容を検証し、成功時だけ編集中のファイルを切り替える。
	std::filesystem::path resolved;

	if (!ResolvePath(path, true, resolved, error)) {
		return false;
	}

	// 全件の読込と検証が済むまで、現在の保存先を変えない。
	if (!SceneFile::Load(world, resolved, error, true)) {
		return false;
	}

	path_ = resolved;

	// 前の配置の速度やプレビューを初期化する。
	world.OnSceneContentsChanged();

	return true;
}

bool SceneDocument::Save(IEditableWorld& world, std::string& error) {
	// 保存先が決まっていることを確認し、現在のファイルへ保存する。
	if (!HasPath()) {
		error = "Choose a scene filename before saving.";

		return false;
	}

	return SaveAs(world, path_, true, error);
}

bool SceneDocument::SaveAs(IEditableWorld& world, const std::filesystem::path& path, bool isOverwriteConfirmed, std::string& error) {
	// 保存先と上書き可否を確認し、書込成功後に現在のパスを更新する。
	std::filesystem::path resolved;

	// resources内のSceneファイルか検証する。
	if (!ResolvePath(path, false, resolved, error)) {
		return false;
	}

	std::error_code fileError;

	// 別の既存ファイルを上書きする場合は確認を要求する。
	if (std::filesystem::exists(resolved, fileError)) {
		const bool isCurrentFile = AssetPathChange::DoesContain(path_, resolved) && AssetPathChange::DoesContain(resolved, path_);

		if (!isCurrentFile) {
			if (!isOverwriteConfirmed) {
				error = "This scene already exists. Confirm overwrite before saving.";

				return false;
			}
		}
	}

	if (fileError) {
		error = fileError.message();

		return false;
	}

	std::filesystem::path temporary = resolved;

	temporary += L".tmp";

	// 復旧に使える既存の一時ファイルを保護する。
	if (GetFileAttributesW(temporary.c_str()) != INVALID_FILE_ATTRIBUTES) {
		error = "A temporary scene file already exists. Recover or move it before saving.";

		return false;
	}

	if (!SceneFile::Save(world, resolved, error)) {
		return false;
	}

	// 保存成功後にだけ、今後の保存先を切り替える。
	path_ = resolved;

	return true;
}

//=============================================================================================//
// パスの検証・更新

bool SceneDocument::ResolvePath(const std::filesystem::path& requested, bool mustExist, std::filesystem::path& resolved, std::string& error) const try {
	resolved.clear();

	error.clear();

	if (root_.empty() || requested.empty()) {
		error = "Choose a .ponscene file inside resources.";

		return false;
	}

	std::filesystem::path candidate = requested;

	if (!candidate.is_absolute()) {
		// APIはresources相対を基本とする。起動時の既存形式resources/...も受け付ける。
		if (*candidate.begin() == "resources") {
			candidate = root_ / candidate.lexically_relative("resources");
		} else {
			candidate = root_ / candidate;
		}
	}

	if (!mustExist) {
		if (candidate.extension().empty()) {
			candidate += L".ponscene";
		}
	}

	candidate = candidate.lexically_normal();

	if (!AssetPathChange::DoesContain(root_, candidate) || candidate == root_) {
		error = "Scene files must stay inside resources.";

		return false;
	}

	std::string extension = AssetPathChange::Text(candidate.extension());

	for (char& character : extension) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	if (extension != ".ponscene") {
		error = "Choose a .ponscene file.";

		return false;
	}

	const std::filesystem::path relative = candidate.lexically_relative(root_);

	if (AssetPathChange::Text(relative).size() > 240) {
		error = "The scene path exceeds 240 UTF-8 bytes.";

		return false;
	}

	std::filesystem::path component = root_;

	for (const std::filesystem::path& part : relative) {
		const std::wstring name = part.native();

		if (name.empty() || name == L".." || name.back() == L'.' || name.back() == L' ') {
			error = "Invalid scene filename.";

			return false;
		}

		if (AssetPathChange::DoesContain(".editor-trash", part) || name.find_first_of(L"<>:\"|?*") != std::wstring::npos) {
			error = "Reserved or invalid scene path.";

			return false;
		}

		for (wchar_t character : name) {
			if (character < 32) {
				error = "Scene filenames cannot contain control characters.";

				return false;
			}
		}

		std::wstring stem = part.stem().native();

		for (wchar_t& character : stem) {
			character = std::towupper(character);
		}

		if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL") {
			error = "Windows reserves this filename.";

			return false;
		}

		if (stem.size() == 4) {
			if (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) {
				if (stem.back() >= L'1' && stem.back() <= L'9') {
					error = "Windows reserves this filename.";

					return false;
				}
			}
		}

		component /= part;

		const DWORD attributes = GetFileAttributesW(component.c_str());

		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				error = "Linked scene files and folders cannot be opened or overwritten.";

				return false;
			}
		}
	}

	std::error_code fileError;

	if (std::filesystem::is_directory(candidate, fileError)) {
		error = "Choose a file, not a folder.";

		return false;
	}

	if (mustExist) {
		if (!std::filesystem::is_regular_file(candidate, fileError)) {
			error = "Scene file does not exist.";

			return false;
		}
	}

	resolved = candidate;

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

void SceneDocument::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	if (HasPath()) {
		path_ = AssetPathChange::Path(change.RemapResource(AssetPathChange::Text(path_)));
	}
}
