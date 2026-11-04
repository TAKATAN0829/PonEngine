#pragma once

#include "AssetDocument.h"
#include "IEditableWorld.h"

// Projectのファイル操作。事前検証・書込準備・確定・失敗時の復元を一か所に集める。
class AssetFileOperations {
public:
	//=============================================================================================//
	// 初期化処理

	bool Initialize(const std::filesystem::path& root, std::string& error);

	//=============================================================================================//
	// 使用箇所の確認

	// ファイル操作時だけ走査する。毎フレームのProject描画では呼ばない。
	bool Review(const std::filesystem::path& source, const std::vector<AssetReference>& liveReferences, std::string& error);

	const std::vector<AssetReference>& GetUsages() const { return usages_; }

	//=============================================================================================//
	// ファイル操作

	bool Move(const std::filesystem::path& source, const std::filesystem::path& destination,
		IEditableWorld& world, const std::vector<AssetReference>& liveReferences, std::string& error);

	// 使用中なら拒否する。物理削除せず、resources内の非表示ゴミ箱へ移す。
	bool MoveToTrash(const std::filesystem::path& source, const std::vector<AssetReference>& liveReferences,
		std::filesystem::path& trashPath, std::string& error);

	bool CreateFolder(const std::filesystem::path& destination, std::string& error);

private:
	//=============================================================================================//
	// 検証・走査

	bool ValidatePath(const std::filesystem::path& path, bool mustExist, std::filesystem::path& relative, std::string& error) const;

	bool Scan(std::string& error);

	bool IsProtected(const std::filesystem::path& relative) const;

	std::filesystem::path CreateRecoveryDirectory(std::string& error) const;

	//=============================================================================================//
	// 調査結果

	std::filesystem::path root_;

	std::vector<AssetDocument> documents_;

	std::vector<std::filesystem::path> assetPaths_; // フォルダ移動後の全子パスも長さを検証する。

	std::vector<AssetReference> usages_;
};
