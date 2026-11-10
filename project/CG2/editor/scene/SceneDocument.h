#pragma once

#include "IWorldObjects.h"
#include "AssetPathChange.h"

class IEditorSceneLifecycle;

// 編集中の配置ファイルと保存先を所有する。履歴とパネルの状態はEditorが管理する。
class SceneDocument {
public:

	//=============================================================================================//
	// 初期化処理

	bool Initialize(const std::filesystem::path& resources, const std::filesystem::path& initialPath, std::string& error);

	//=============================================================================================//
	// Getter / 編集中のファイル

	const std::filesystem::path& GetRoot() const;

	const std::filesystem::path& GetPath() const;

	bool HasPath() const;

	std::string GetName() const;

	//=============================================================================================//
	// 新規・読込・保存

	bool New(IWorldObjects& objects, IEditorSceneLifecycle& lifecycle, std::string& error);

	// 成功したときだけ現在のファイルを切り替える。
	bool Open(IWorldObjects& objects, IEditorSceneLifecycle& lifecycle, const std::filesystem::path& path, std::string& error);

	bool Save(IWorldObjects& objects, std::string& error);

	// 別ファイルの上書きは確認済みの場合だけ許可する。
	bool SaveAs(IWorldObjects& objects, const std::filesystem::path& path, bool isOverwriteConfirmed, std::string& error);

	//=============================================================================================//
	// パスの検証・更新

	// 保存時だけ拡張子省略を補完する。resources外・リンク・ゴミ箱を拒否する。
	bool ResolvePath(const std::filesystem::path& requested, bool mustExist, std::filesystem::path& resolved, std::string& error) const;

	void RemapAssetPaths(const AssetPathChange& change);

private:
	//=============================================================================================//
	// 保存先

	std::filesystem::path root_;
	std::filesystem::path path_; // 空なら未保存。新規作成で以前の保存先を引き継がない。
};
