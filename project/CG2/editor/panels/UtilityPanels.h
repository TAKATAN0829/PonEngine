#pragma once
#include "IEditableScene.h"
#include <filesystem>
#include <unordered_map>
class EditorSelection;

// Projectのフォルダ・サムネイルとConsoleを表示する。
class UtilityPanels {
public:
	//=============================================================================================//
	// 初期化・更新・描画

	void Initialize();
	void Update(); // フォルダを変更・更新した時だけディレクトリを走査する。
	void Draw(IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility);

private:
	//=============================================================================================//
	// 型 / アセット一覧

	struct AssetEntry {
		std::filesystem::path path;
		std::string name;
		std::string extension;
		bool directory = false;
	};
	struct Preview {
		uint64_t handle = 0;
		float width = 1;
		float height = 1;
	};
	static bool CompareEntries(const AssetEntry& first, const AssetEntry& second);
	static std::string PathText(const std::filesystem::path& path);
	void OpenDirectory(const std::filesystem::path& path);
	void DrawProject(IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility);
	void DrawAsset(const AssetEntry& entry, IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility);
	void DrawAssetActions(const AssetEntry& entry, IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility);
	void DrawConsole();
	Preview GetPreview(const AssetEntry& entry, IEditableScene& scene);

private:
	//=============================================================================================//
	// メンバ変数 / フォルダ・選択・プレビュー

	std::filesystem::path root_;
	std::filesystem::path directory_;
	std::filesystem::path selected_;
	std::vector<AssetEntry> entries_;
	std::unordered_map<std::string, Preview> previews_; // TextureManagerが画像を所有する。
	char search_[128]{};
	std::string status_;
	bool initialized_ = false;
	bool refreshRequested_ = false;
	bool previewLoadedThisFrame_ = false; // 一度に大量の画像を読み込まない。
};
