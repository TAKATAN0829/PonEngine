#include "AssetPathChange.h"
#include <Windows.h>

//=============================================================================================//
// 実装内補助定義

namespace {
	std::filesystem::path Normalize(const std::filesystem::path& path) {
		// パスの区切りと余分な要素を整理し、比較できる形へ揃える。
		std::filesystem::path normalized = path.lexically_normal();

		while (normalized.has_relative_path() && normalized.filename().empty()) {
			normalized = normalized.parent_path();
		}

		return normalized;
	}
}

//=============================================================================================//
// パスの比較・変換

bool AssetPathChange::DoesContain(const std::filesystem::path& parent, const std::filesystem::path& child) {
	const std::filesystem::path normalizedParent = Normalize(parent);
	const std::filesystem::path normalizedChild = Normalize(child);

	std::filesystem::path::const_iterator parentPart = normalizedParent.begin();
	std::filesystem::path::const_iterator childPart = normalizedChild.begin();

	while (parentPart != normalizedParent.end()) {
		if (childPart == normalizedChild.end()) {
			return false;
		}

		const std::wstring parentName = parentPart->native();
		const std::wstring childName = childPart->native();

		// Windowsと同じ序数比較を使い、日本語を含むパスでもCロケールに依存しない。
		if (CompareStringOrdinal(parentName.c_str(), static_cast<int>(parentName.size()),
			childName.c_str(), static_cast<int>(childName.size()), TRUE) != CSTR_EQUAL) {
			return false;
		}

		++parentPart;

		++childPart;
	}

	return true;
}

std::string AssetPathChange::Text(const std::filesystem::path& path) {
	// ファイルパスを、保存や表示に使うUTF-8の文字列へ変換する。
	const std::u8string text = path.generic_u8string();

	return std::string(text.begin(), text.end());
}

std::filesystem::path AssetPathChange::Path(std::string_view text) {
	// UTF-8の文字列を、Windowsのファイル操作で使うパスへ変換する。
	return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::string AssetPathChange::RemapRelative(const std::string& path) const {
	if (path.empty()) {
		return path;
	}

	const std::filesystem::path original(std::u8string(path.begin(), path.end()));

	if (!DoesContain(source, original)) {
		return path;
	}

	// lexically_relativeの大文字・小文字比較に依存せず、一致済みの要素だけを取り除く。
	const std::filesystem::path normalized = Normalize(original);
	const std::filesystem::path normalizedSource = Normalize(source);

	std::filesystem::path::const_iterator suffix = normalized.begin();

	for (const std::filesystem::path& part : normalizedSource) {
		(void)part;

		++suffix;
	}

	std::filesystem::path result = destination;

	while (suffix != normalized.end()) {
		result /= *suffix;

		++suffix;
	}

	return Text(result.lexically_normal());
}

std::string AssetPathChange::RemapResource(const std::string& path) const {
	// resources相対と絶対パスを区別し、移動対象の参照だけを更新する。
	const std::filesystem::path original(std::u8string(path.begin(), path.end()));

	if (original.is_absolute()) {
		if (!DoesContain(resourcesRoot, original)) {
			return path;
		}

		const std::string relative = Text(Normalize(original).lexically_relative(resourcesRoot));
		const std::string remapped = RemapRelative(relative);

		if (relative == remapped) {
			return path;
		}

		return Text(resourcesRoot / AssetPathChange::Path(remapped));
	}

	const std::filesystem::path normalized = Normalize(original);

	if (normalized.empty()) {
		return path;
	}

	if (*normalized.begin() != "resources") {
		return path;
	}

	const std::string relative = Text(normalized.lexically_relative("resources"));
	const std::string remapped = RemapRelative(relative);

	if (relative == remapped) {
		return path;
	}

	return "resources/" + remapped;
}
