#pragma once

#include "objects/WorldAssetReferences.h"
#include <filesystem>

// 保存形式の参照と、モデル内の相対パスを扱う。ファイル操作の確定は担当しない。
class AssetDocument {
public:
	//=============================================================================================//
	// 読込・更新ファイルの準備

	bool Load(const std::filesystem::path& root, const std::filesystem::path& relative, std::string& error);

	bool WriteUpdated(const std::filesystem::path& stagingPath, const AssetPathChange& change, std::string& error) const;

	//=============================================================================================//
	// Getter / 原本・参照

	const std::filesystem::path& GetPath() const;

	const std::string& GetOriginal() const;

	const std::vector<AssetReference>& GetReferences() const;

	bool IsOpaque() const;

	bool HasChanges(const AssetPathChange& change) const;

private:
	//=============================================================================================//
	// 型 / 外部ファイルへのリンク

	struct Link {
		size_t begin;
		size_t length;
		std::filesystem::path target;
		bool isUri;
	};

	//=============================================================================================//
	// モデル内の参照

	bool ReadJsonLinks(std::string& error);

	bool ReadWavefrontLinks(std::string& error);

	bool AddLink(size_t begin, size_t length, const std::string& value, bool isUri, std::string& error);

	std::string GetUpdatedLink(const Link& link, const AssetPathChange& change) const;

	//=============================================================================================//
	// 原本と検証済みデータ

	std::filesystem::path root_;
	std::filesystem::path path_;

	std::string extension_;
	std::string original_;
	std::string text_;
	std::string binarySuffix_; // GLBのBIN等のチャンクは変更しない。

	std::vector<WorldObjectData> objects_;

	ParticleSettings particle_;

	std::vector<AssetReference> references_;

	std::vector<Link> links_;
};
