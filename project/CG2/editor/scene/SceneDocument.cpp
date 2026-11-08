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
	// 入力されたパスと、検証後に使うパスを分けて持つ。
	std::filesystem::path resolved;

	// 第2引数の true で、既存の .ponscene ファイルを必須にする。
	if (!ResolvePath(path, true, resolved, error)) {
		// パスを確定できなければ、ファイルを開く前に中断する。
		return false;
	}

	// 検証済みのパスを、ファイルを開いて内容を解析する SceneFile に渡す。
	// 最後の true は、旧形式のライト設定を現在の形式に移行する指定。
	if (!SceneFile::Load(world, resolved, error, true)) {
		// SceneFileが書いた失敗理由をそのまま呼び出し元へ返す。
		return false;
	}

	// 開けたファイルを、以後の上書き保存の対象にする。
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
		// 両方向に包含するか調べ、同じパスと親子のパスを区別する。
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
	// 検証に失敗したときに、前回のパスが出力先に残らないようにする。
	resolved.clear();

	// 今回の失敗理由を返せるように、前回のメッセージを消す。
	error.clear();

	// resources の基準位置と、利用者が指定したパスの両方が必要。
	if (root_.empty() || requested.empty()) {
		error = "Choose a .ponscene file inside resources.";

		return false;
	}

	// 元の入力を保持したまま、検証用のパスを組み立てる。
	std::filesystem::path candidate = requested;

	if (!candidate.is_absolute()) {
		// APIはresources相対を基本とする。起動時の既存形式resources/...も受け付ける。
		if (*candidate.begin() == "resources") {
			// 先頭の resources を取り除き、基準パスとの二重指定を防ぐ。
			candidate = root_ / candidate.lexically_relative("resources");
		} else {
			// resources からの相対パスを、ファイル操作に使う絶対パスへ変換する。
			candidate = root_ / candidate;
		}
	}

	// 保存時だけ拡張子の省略を許可する。読込時は入力されたファイルを探す。
	if (!mustExist) {
		if (candidate.extension().empty()) {
			candidate += L".ponscene";
		}
	}

	// . や .. を整理したパスで、resourcesの内側に収まるかを調べる。
	candidate = candidate.lexically_normal();

	// resources の外を指すパスと、resources 自体を指すパスを除く。
	if (!AssetPathChange::DoesContain(root_, candidate) || candidate == root_) {
		error = "Scene files must stay inside resources.";

		return false;
	}

	// 拡張子だけを取り出し、実際のファイル名はそのまま保持する。
	std::string extension = AssetPathChange::Text(candidate.extension());

	// .PONSCENE なども同じ形式として扱うため、小文字で比較する。
	for (char& character : extension) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	if (extension != ".ponscene") {
		error = "Choose a .ponscene file.";

		return false;
	}

	// 基準パスを取り除き、利用者が指定したフォルダ名とファイル名を検査する。
	const std::filesystem::path relative = candidate.lexically_relative(root_);

	// パスの上限は UTF-8 に変換した後のバイト数で判定する。
	if (AssetPathChange::Text(relative).size() > 240) {
		error = "The scene path exceeds 240 UTF-8 bytes.";

		return false;
	}

	std::filesystem::path component = root_;

	// 最後のファイル名だけでなく、そこへ至るフォルダ名も順番に確認する。
	for (const std::filesystem::path& part : relative) {
		// Windows のパス表現で、区切りごとの名前を取り出す。
		const std::wstring name = part.native();

		// 空の名前、親フォルダの指定、末尾のピリオドや空白を拒否する。
		if (name.empty() || name == L".." || name.back() == L'.' || name.back() == L' ') {
			error = "Invalid scene filename.";

			return false;
		}

		// Editor のゴミ箱と、Windows のファイル名に使えない記号を除く。
		if (AssetPathChange::DoesContain(".editor-trash", part) || name.find_first_of(L"<>:\"|?*") != std::wstring::npos) {
			error = "Reserved or invalid scene path.";

			return false;
		}

		// タブや改行など、目で判別しにくい制御文字も除く。
		for (wchar_t character : name) {
			if (character < 32) {
				error = "Scene filenames cannot contain control characters.";

				return false;
			}
		}

		// NUL.ponscene のように拡張子を付けた名前も、予約名の検査対象にする。
		std::wstring stem = part.stem().native();

		// 大文字と小文字の違いを無視して予約名と比較する。
		for (wchar_t& character : stem) {
			character = std::towupper(character);
		}

		if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL") {
			error = "Windows reserves this filename.";

			return false;
		}

		// COM1〜COM9 と LPT1〜LPT9 も Windows が予約している。
		if (stem.size() == 4) {
			if (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) {
				if (stem.back() >= L'1' && stem.back() <= L'9') {
					error = "Windows reserves this filename.";

					return false;
				}
			}
		}

		// パスを一段ずつ伸ばし、途中のフォルダにも属性検査を行う。
		component /= part;

		// 見かけ上はresources内でも、途中のリンクで別の場所へ飛ぶことを防ぐ。
		const DWORD attributes = GetFileAttributesW(component.c_str());

		// 属性を取得できたときだけ、リンクかどうかをビットで調べる。
		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				error = "Linked scene files and folders cannot be opened or overwritten.";

				return false;
			}
		}
	}

	// ファイルの種類を調べるときのエラーを受け取る。
	std::error_code fileError;

	// 拡張子が .ponscene でも、実体がフォルダなら開けない。
	if (std::filesystem::is_directory(candidate, fileError)) {
		error = "Choose a file, not a folder.";

		return false;
	}

	// 読み込み時は、パスの先に通常のファイルが存在することまで確認する。
	if (mustExist) {
		if (!std::filesystem::is_regular_file(candidate, fileError)) {
			error = "Scene file does not exist.";

			return false;
		}
	}

	// すべての検証を通ったパスを、読み込み側へ返す。
	resolved = candidate;

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	// パス操作が例外を出した場合も、画面に表示できるメッセージを返す。
	error = failure.what();

	return false;
}

void SceneDocument::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	if (HasPath()) {
		path_ = AssetPathChange::Path(change.RemapResource(AssetPathChange::Text(path_)));
	}
}
