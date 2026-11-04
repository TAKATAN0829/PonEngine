#include "AssetFileOperations.h"
#include "objects/WorldObjects.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <cwctype>

//=============================================================================================//
// 実装内補助定義

namespace {
	std::string LowerExtension(const std::filesystem::path& path) {
		std::string extension = AssetPathChange::Text(path.extension());

		for (char& character : extension) {
			character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
		}

		return extension;
	}

	bool ReadBytes(const std::filesystem::path& path, std::string& bytes) {
		std::ifstream input(path, std::ios::binary);

		if (!input) {
			return false;
		}

		bytes.assign(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());

		return !input.bad();
	}

	bool WriteBytes(const std::filesystem::path& path, const std::string& bytes) {
		std::ofstream output(path, std::ios::binary | std::ios::trunc);

		output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));

		output.close();

		return static_cast<bool>(output);
	}

	struct StagedDocument {
		std::filesystem::path original;
		std::filesystem::path destination;
		std::filesystem::path backup;
		std::filesystem::path updated;

		bool applied = false;
	};
}

//=============================================================================================//
// 初期化処理

bool AssetFileOperations::Initialize(const std::filesystem::path& root, std::string& error) {
	error.clear();

	std::error_code fileError;

	root_ = std::filesystem::canonical(root, fileError);

	if (fileError) {
		error = "Cannot resolve resources: " + fileError.message();

		return false;
	}

	return true;
}

//=============================================================================================//
// 使用箇所の確認

bool AssetFileOperations::Review(const std::filesystem::path& source, const std::vector<AssetReference>& liveReferences, std::string& error) try {
	error.clear();

	usages_.clear();

	std::filesystem::path relative;

	if (!ValidatePath(source, true, relative, error)) {
		return false;
	}

	if (!Scan(error)) {
		return false;
	}

	for (const AssetReference& reference : liveReferences) {
		if (AssetPathChange::Contains(relative, AssetPathChange::Path(reference.path))) {
			usages_.push_back(reference);
		}
	}

	for (const AssetDocument& document : documents_) {
		// 同じフォルダごとゴミ箱へ移す参照元は、外部からの使用箇所には含めない。
		if (!AssetPathChange::Contains(relative, document.GetPath())) {
			for (const AssetReference& reference : document.GetReferences()) {
				if (AssetPathChange::Contains(relative, AssetPathChange::Path(reference.path))) {
					usages_.push_back(reference);
				}
			}
		}

		if (document.IsOpaque() && !AssetPathChange::Contains(relative, document.GetPath())) {
			const std::string extension = LowerExtension(relative);

			if (std::filesystem::is_directory(root_ / relative) || extension == ".fbx" || extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".dds" || extension == ".tga") {
				usages_.push_back({AssetPathChange::Text(relative), "FBX dependency inspection is unavailable: " + AssetPathChange::Text(document.GetPath())});
			}
		}
	}

	if (IsProtected(relative)) {
		usages_.push_back({AssetPathChange::Text(relative), "Engine / built-in asset or default save path"});
	}

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

//=============================================================================================//
// ファイル操作

bool AssetFileOperations::Move(const std::filesystem::path& source, const std::filesystem::path& destination,
	IEditableWorld& world, const std::vector<AssetReference>& liveReferences, std::string& error) try {
	if (!Review(source, liveReferences, error)) {
		return false;
	}

	AssetPathChange change;

	change.resourcesRoot = root_;

	if (!ValidatePath(source, true, change.source, error)) {
		return false;
	}

	if (!ValidatePath(destination, false, change.destination, error)) {
		return false;
	}

	if (IsProtected(change.source)) {
		error = "Engine built-in assets and default save paths cannot be moved.";

		return false;
	}

	for (const AssetReference& usage : usages_) {
		if (usage.owner.starts_with("FBX dependency inspection")) {
			error = usage.owner;

			return false;
		}
	}

	for (const AssetDocument& document : documents_) {
		if (document.IsOpaque()) {
			if (AssetPathChange::Contains(change.source, document.GetPath())) {
				error = "FBX file relocation requires dependency-aware re-export.";

				return false;
			}
		}
	}

	if (change.source == change.destination) {
		error = "The destination is unchanged.";

		return false;
	}

	if (AssetPathChange::Contains(change.source, change.destination)) {
		if (!AssetPathChange::Contains(change.destination, change.source)) {
			error = "A folder cannot be moved inside itself.";

			return false;
		}
	}

	std::error_code fileError;

	const bool directory = std::filesystem::is_directory(root_ / change.source, fileError);

	if (!directory) {
		if (LowerExtension(change.source) != LowerExtension(change.destination)) {
			error = "Keep the original file extension when renaming.";

			return false;
		}
	}

	if (std::filesystem::exists(root_ / change.destination)) {
		if (!std::filesystem::equivalent(root_ / change.source, root_ / change.destination)) {
			error = "Another file or folder already exists at the destination.";

			return false;
		}
	}

	if (!std::filesystem::is_directory((root_ / change.destination).parent_path())) {
		error = "The destination folder does not exist.";

		return false;
	}

	for (const std::filesystem::path& asset : assetPaths_) {
		if (change.RemapRelative(AssetPathChange::Text(asset)).size() > 240) {
			error = "A child asset path exceeds 240 UTF-8 bytes after the move.";

			return false;
		}
	}

	for (const AssetReference& reference : liveReferences) {
		if (change.RemapRelative(reference.path).size() > 240) {
			error = "A history reference would exceed the supported path length.";

			return false;
		}
	}

	// 保存可能な長さ・Component設定を、ファイルを触る前に確認する。
	std::vector<WorldObjectData> candidate = world.CaptureObjects();

	WorldAssetReferences::Remap(candidate, change);

	WorldObjects validator;

	if (!validator.ReplaceObjects(std::move(candidate), error, false)) {
		error = "Current scene: " + error;

		return false;
	}

	if (world.GetParticleSettings() != nullptr) {
		ParticleSettings particle = *world.GetParticleSettings();

		particle.texture = change.RemapResource(particle.texture);

		if (!ValidateParticleSettings(particle)) {
			error = "The destination path cannot be stored in Particle settings.";

			return false;
		}
	}

	const std::filesystem::path recovery = CreateRecoveryDirectory(error);

	if (recovery.empty()) {
		return false;
	}

	std::vector<StagedDocument> staged;

	for (const AssetDocument& document : documents_) {
		std::string current;

		if (!ReadBytes(root_ / document.GetPath(), current) || current != document.GetOriginal()) {
			error = "An asset changed during reference inspection. Try again.";

			return false;
		}

		if (!document.HasChanges(change)) {
			continue;
		}

		StagedDocument file;

		file.original = root_ / document.GetPath();

		file.destination = root_ / AssetPathChange::Path(change.RemapRelative(AssetPathChange::Text(document.GetPath())));

		file.backup = recovery / (std::to_string(staged.size()) + ".original");

		file.updated = recovery / (std::to_string(staged.size()) + ".updated");

		if (!WriteBytes(file.backup, document.GetOriginal())) {
			error = "Cannot create a recovery copy.";

			return false;
		}

		if (!document.WriteUpdated(file.updated, change, error)) {
			error = AssetPathChange::Text(document.GetPath()) + ": " + error;

			return false;
		}

		staged.push_back(std::move(file));
	}

	std::ostringstream manifest;

	manifest << "Source: " << AssetPathChange::Text(change.source) << "\nDestination: " << AssetPathChange::Text(change.destination) << '\n';

	for (const StagedDocument& file : staged) {
		manifest << AssetPathChange::Text(file.backup.filename()) << " -> " << AssetPathChange::Text(file.original.lexically_relative(root_)) << '\n';
	}

	if (!WriteBytes(recovery / "recovery.txt", manifest.str())) {
		error = "Cannot write recovery information.";

		return false;
	}

	// 二段階のrenameで、大文字・小文字だけの名前変更も扱う。
	const std::filesystem::path holding = recovery / "moving";

	std::filesystem::rename(root_ / change.source, holding, fileError);

	if (fileError) {
		error = "Cannot move asset: " + fileError.message();

		return false;
	}

	std::filesystem::rename(holding, root_ / change.destination, fileError);

	bool success = !fileError;

	if (success) {
		for (StagedDocument& file : staged) {
			if (!MoveFileExW(file.updated.c_str(), file.destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
				success = false;

				break;
			}

			file.applied = true;
		}
	}

	if (!success) {
		bool restored = true;

		for (StagedDocument& file : staged) {
			if (file.applied) {
				if (!MoveFileExW(file.backup.c_str(), file.destination.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
					restored = false;
				}
			}
		}

		if (std::filesystem::exists(holding)) {
			std::filesystem::rename(holding, root_ / change.source, fileError);
		} else {
			std::filesystem::rename(root_ / change.destination, root_ / change.source, fileError);
		}

		error = "Asset update failed. Recovery copies: " + AssetPathChange::Text(recovery);

		if (fileError || !restored) {
			error += " / Automatic restoration was incomplete; keep the recovery directory.";
		}

		return false;
	}

	// GPUリソースを作り直さず、既存キャッシュと編集データの参照名だけを切り替える。
	world.RemapAssetPaths(change);

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

bool AssetFileOperations::MoveToTrash(const std::filesystem::path& source, const std::vector<AssetReference>& liveReferences,
	std::filesystem::path& trashPath, std::string& error) try {
	trashPath.clear();

	if (!Review(source, liveReferences, error)) {
		return false;
	}

	if (!usages_.empty()) {
		error = "This asset is referenced. Remove the displayed references before deleting it.";

		return false;
	}

	std::filesystem::path relative;

	if (!ValidatePath(source, true, relative, error)) {
		return false;
	}

	const std::filesystem::path recovery = CreateRecoveryDirectory(error);

	if (recovery.empty()) {
		return false;
	}

	if (!WriteBytes(recovery / "recovery.txt", "Deleted from: " + AssetPathChange::Text(relative) + "\n")) {
		error = "Cannot write trash recovery information.";

		return false;
	}

	std::error_code fileError;

	trashPath = recovery / relative.filename();

	std::filesystem::rename(root_ / relative, trashPath, fileError);

	if (fileError) {
		error = fileError.message();

		trashPath.clear();

		return false;
	}

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

bool AssetFileOperations::CreateFolder(const std::filesystem::path& destination, std::string& error) try {
	std::filesystem::path relative;

	if (!ValidatePath(destination, false, relative, error)) {
		return false;
	}

	std::error_code fileError;

	if (!std::filesystem::create_directory(root_ / relative, fileError)) {
		error = "Cannot create folder: " + fileError.message();

		return false;
	}

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	error = failure.what();

	return false;
}

//=============================================================================================//
// 検証・走査

bool AssetFileOperations::ValidatePath(const std::filesystem::path& path, bool mustExist, std::filesystem::path& relative, std::string& error) const {
	error.clear();

	if (root_.empty()) {
		error = "Asset workspace has not been initialized.";

		return false;
	}

	std::filesystem::path absolute = path;

	if (!absolute.is_absolute()) {
		absolute = root_ / path;
	}

	absolute = absolute.lexically_normal();

	relative = absolute.lexically_relative(root_);

	if (!AssetPathChange::Contains(root_, absolute) || relative.empty() || relative == ".") {
		error = "Choose an asset inside resources; the resources root cannot be modified.";

		return false;
	}

	std::filesystem::path current = root_;

	for (const std::filesystem::path& part : relative) {
		const std::wstring name = part.native();

		if (name.empty() || name == L".." || AssetPathChange::Contains(".editor-trash", part) || name.back() == L'.' || name.back() == L' ') {
			error = "Invalid or reserved asset path.";

			return false;
		}

		if (name.find_first_of(L"<>:\"|?*") != std::wstring::npos) {
			error = "The name contains invalid characters.";

			return false;
		}

		for (wchar_t character : name) {
			if (character < 32) {
				error = "The name contains a control character.";

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

		current /= part;

		std::error_code fileError;

		if (std::filesystem::is_symlink(std::filesystem::symlink_status(current, fileError))) {
			error = "Symbolic links and linked folders cannot be edited.";

			return false;
		}

		// Junctionも含む再解析ポイントを拒否し、resources外への書込を防ぐ。
		const DWORD attributes = GetFileAttributesW(current.c_str());

		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				error = "Reparse points cannot be edited.";

				return false;
			}
		}
	}

	if (AssetPathChange::Text(relative).size() > 240) {
		error = "The resource path exceeds 240 UTF-8 bytes.";

		return false;
	}

	if (mustExist) {
		if (!std::filesystem::exists(absolute)) {
			error = "The asset no longer exists.";

			return false;
		}
	}

	return true;
}

bool AssetFileOperations::Scan(std::string& error) {
	documents_.clear();

	assetPaths_.clear();

	size_t inspectedBytes = 0;

	std::error_code fileError;

	std::filesystem::recursive_directory_iterator iterator(root_, fileError);

	const std::filesystem::recursive_directory_iterator end;

	size_t fileCount = 0;

	while (!fileError && iterator != end) {
		const std::filesystem::path path = iterator->path();

		if (AssetPathChange::Contains(".editor-trash", path.filename())) {
			iterator.disable_recursion_pending();
		} else {
			const DWORD attributes = GetFileAttributesW(path.c_str());

			if (attributes != INVALID_FILE_ATTRIBUTES) {
				if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
					error = "Reference inspection cannot follow a linked asset: " + AssetPathChange::Text(path);

					return false;
				}
			}

			if (iterator->is_regular_file(fileError)) {
				assetPaths_.push_back(path.lexically_relative(root_));

				++fileCount;

				if (fileCount > 10000) {
					error = "Asset inspection exceeds 10000 files.";

					return false;
				}

				const std::string extension = LowerExtension(path);

				if (extension == ".ponscene" || extension == ".ponprefab" || extension == ".ponparticle" || extension == ".obj" || extension == ".mtl" || extension == ".gltf" || extension == ".glb" || extension == ".fbx") {
					AssetDocument document;

					if (!document.Load(root_, path.lexically_relative(root_), error)) {
						error = AssetPathChange::Text(path.lexically_relative(root_)) + ": " + error;

						return false;
					}

					inspectedBytes += document.GetOriginal().size();

					if (inspectedBytes > 128 * 1024 * 1024) {
						error = "Reference inspection exceeds 128 MiB of documents.";

						return false;
					}

					documents_.push_back(std::move(document));
				}
			}
		}

		iterator.increment(fileError);
	}

	if (fileError) {
		error = "Cannot inspect all assets: " + fileError.message();

		return false;
	}

	return true;
}

bool AssetFileOperations::IsProtected(const std::filesystem::path& relative) const {
	static const char* builtins[] = {
		"plane.gltf", "axis.obj", "circle.png", "white1x1.png", "uvChecker.png",
		"primitives/Cube.obj", "primitives/Sphere.obj", "scenes/GameScene.ponscene",
		"particles/Default.ponparticle", "shaders"
	};

	for (const char* builtin : builtins) {
		if (AssetPathChange::Contains(relative, builtin)) {
			return true;
		}

		if (AssetPathChange::Contains("shaders", relative)) {
			return true;
		}
	}

	return false;
}

std::filesystem::path AssetFileOperations::CreateRecoveryDirectory(std::string& error) const {
	const std::filesystem::path trashRoot = root_ / ".editor-trash";

	const DWORD attributes = GetFileAttributesW(trashRoot.c_str());

	if (attributes != INVALID_FILE_ATTRIBUTES) {
		if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
			error = "The recovery directory is a reparse point.";

			return {};
		}
	}

	std::error_code fileError;

	std::filesystem::create_directories(trashRoot, fileError);

	if (fileError) {
		error = "Cannot create recovery directory: " + fileError.message();

		return {};
	}

	const int64_t timestamp = std::chrono::system_clock::now().time_since_epoch().count();

	for (size_t attempt = 0; attempt < 100; ++attempt) {
		const std::filesystem::path directory = trashRoot / (std::to_string(timestamp) + "-" + std::to_string(attempt));

		if (std::filesystem::create_directory(directory, fileError)) {
			return directory;
		}

		if (fileError) {
			error = fileError.message();

			return {};
		}
	}

	error = "Cannot allocate a recovery directory.";

	return {};
}
