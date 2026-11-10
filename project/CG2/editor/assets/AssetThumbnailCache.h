#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

class IEditableWorld;

// 描画先が所有するサムネイルを借りるための情報。ハンドルが0なら代替アイコンを使う。
struct AssetThumbnail {
	AssetThumbnail();
	uint64_t handle;
	float width;
	float height;
};

// Projectで表示するモデルと画像のサムネイルを、同じ窓口から取得する。
class AssetThumbnailCache {
public:

	//=============================================================================================//
	// 更新処理

	// 未取得のAssetを準備できる回数をフレームごとに戻す。
	void BeginFrame();

	// Assetの移動や改名で古くなったキャッシュを破棄する。
	void Clear();

	//=============================================================================================//
	// Getter / サムネイル

	// 一覧で取得した小文字の拡張子を使う。未対応の形式や準備待ちの場合は空の情報を返す。
	AssetThumbnail GetThumbnail(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot,
		const std::string& extension, IEditableWorld& scene, std::string& error);

private:

	//=============================================================================================//
	// キャッシュ・フレーム内の準備状態

	static constexpr std::size_t kCacheLimitForImages = 64; // 画像を追加できるキャッシュ件数の上限。

	std::unordered_map<std::string, AssetThumbnail> thumbnails_;

	bool wasThumbnailRequestedThisFrame_ = false;
};
