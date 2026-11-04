#pragma once

#include <filesystem>
#include <string>
#include <string_view>

// ファイル移動の前後を表す。ファイルシステムやGPUには変更を加えない。
struct AssetPathChange {
	std::filesystem::path resourcesRoot;
	std::filesystem::path source;
	std::filesystem::path destination;

	//=============================================================================================//
	// パスの比較・変換

	// 同名の接頭辞ではなく、フォルダの境界で比較する。
	static bool Contains(const std::filesystem::path& parent, const std::filesystem::path& child);

	static std::string Text(const std::filesystem::path& path);

	// UTF-8の表示文字列をWindowsでも文字化けせずパスへ戻す。
	static std::filesystem::path Path(std::string_view text);

	std::string RemapRelative(const std::string& path) const;

	// resources接頭辞や絶対パスで保持するテクスチャにも対応する。
	std::string RemapResource(const std::string& path) const;
};

// 使用箇所の表示に必要な文字列だけを保持し、編集中のデータへのポインタを持たない。
struct AssetReference {
	std::string path;
	std::string owner;
};
