#pragma once
#include "IEditableWorld.h"
#include <filesystem>
#include <unordered_map>
#include <vector>

class EditorSelection;

// Projectだけの状態を所有する。画像・モデルのGPUリソースは各Managerから借用する。
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
	void Draw(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility);

private:
	//=============================================================================================//
	// 型 / 一覧とプレビュー

	struct AssetEntry {
		std::filesystem::path path;
		std::string name;
		std::string extension;
		bool directory = false;
	};
	struct FolderEntry {
		std::filesystem::path path;
		size_t depth = 0;
		bool children = false;
	};
	struct Preview {
		uint64_t handle = 0;
		float width = 1;
		float height = 1;
	};

	//=============================================================================================//
	// ディレクトリ管理

	static bool CompareEntries(const AssetEntry& first, const AssetEntry& second);
	static std::string PathText(const std::filesystem::path& path);
	void OpenDirectory(const std::filesystem::path& path);
	void CacheFolders(const std::filesystem::path& directory, size_t depth);

	//=============================================================================================//
	// 画面構成

	void DrawContents(IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility);
	void DrawFolders();
	void DrawDirectoryActions(); // パンくず側とAsset一覧側で共通の右クリック操作。

	//=============================================================================================//
	// Asset操作・プレビュー

	void DrawAsset(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility);
	void DrawAssetActions(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility);
	Preview GetPreview(const AssetEntry& entry, IEditableWorld& scene);

private:
	//=============================================================================================//
	// メンバ変数 / フォルダ・選択・検索

	std::filesystem::path root_;
	std::filesystem::path directory_;
	std::filesystem::path selected_;
	std::vector<AssetEntry> entries_;
	std::vector<FolderEntry> folders_;
	char search_[128]{};
	bool revealFolder_ = true;

	//=============================================================================================//
	// メンバ変数 / 更新要求・プレビュー

	std::unordered_map<std::string, Preview> previews_; // Managerが所有する画像ハンドルをキャッシュする。
	std::string status_;
	bool initialized_ = false;
	bool refreshRequested_ = false;
	bool previewLoadedThisFrame_ = false; // 画像・モデルの読込は一フレーム一件まで。
};
