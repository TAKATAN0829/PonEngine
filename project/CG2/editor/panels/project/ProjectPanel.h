#pragma once

#include "IEditableWorld.h"
#include "../../assets/AssetActionDialog.h"
#include "../../assets/AssetThumbnailCache.h"
#include <filesystem>
#include <vector>
#include <optional>

class EditorSelection;
class EditorSession;
class ParticleEditorPanel;

// Projectの一覧と操作を管理し、取得したサムネイルをタイルへ表示する。
class ProjectPanel {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize();

	//=============================================================================================//
	// 更新処理 / 一覧のキャッシュ

	// 初期化・フォルダ変更・明示更新のときだけディレクトリを走査する。
	void Update();

	//=============================================================================================//
	// 描画処理

	// 初回の初期化と一覧更新も含め、Project一画面の処理をまとめる。
	void Draw(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility,
		EditorSession& history, ParticleEditorPanel& particles, const std::filesystem::path& activeScenePath = {});

	//=============================================================================================//
	// 操作要求

	bool ConsumeOpenScene(std::filesystem::path& path);

	bool ConsumeAssetPathChange(AssetPathChange& change);

	void RequestRefresh();

	void RevealAsset(const std::filesystem::path& path);

private:

	//=============================================================================================//
	// 型 / 一覧

	struct AssetEntry {
		std::filesystem::path path;

		std::string name;
		std::string extension;

		bool isDirectory;
	};

	struct FolderEntry {
		std::filesystem::path path;

		size_t depth;

		bool hasChildren;
	};

	//=============================================================================================//
	// ディレクトリ管理

	static bool ShouldSortEntryBefore(const AssetEntry& first, const AssetEntry& second);

	static std::string PathText(const std::filesystem::path& path);

	void OpenDirectory(const std::filesystem::path& path);

	void CacheFolders(const std::filesystem::path& directory, size_t depth);

	//=============================================================================================//
	// 画面構成

	void DrawContents(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility,
		EditorSession& history, ParticleEditorPanel& particles, const std::filesystem::path& activeScenePath);

	void DrawFolders();

	void DrawDirectoryActions(); // パンくず側とAsset一覧側で共通の右クリック操作。

	//=============================================================================================//
	// Asset操作・プレビュー

	void DrawAsset(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility);

	void DrawAssetActions(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility);

private:

	//=============================================================================================//
	// フォルダ・選択・検索

	std::filesystem::path root_;
	std::filesystem::path directory_;
	std::filesystem::path selected_;

	std::filesystem::path requestedScene_; // ダブルクリック結果をEditorへ一度だけ渡す。

	std::optional<AssetPathChange> changedAssetPath_;

	std::vector<AssetEntry> entries_;

	std::vector<FolderEntry> folders_;

	char search_[128]{};

	bool shouldRevealFolder_ = true;

	//=============================================================================================//
	// 更新要求・プレビュー

	AssetThumbnailCache thumbnails_; // モデルと画像の取得をまとめる。

	std::string status_;

	AssetActionDialog fileActions_; // ファイル操作と確認画面。シーンのGPUリソースは所有しない。

	bool areFileOperationsAllowed_ = false; // 再生中は閲覧だけを許可する。

	bool isInitialized_ = false;
	bool isRefreshRequested_ = false;
};
