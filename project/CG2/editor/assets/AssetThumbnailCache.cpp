#include "AssetThumbnailCache.h"
#include "AssetPathChange.h"
#include "IEditableWorld.h"
#include "TextureManager.h"
#include "objects/WorldObjects.h"

//=============================================================================================//
// 更新処理

void AssetThumbnailCache::BeginFrame() {
	// 新しく準備するAssetをこのフレームで一件だけ許可する。
	wasThumbnailRequestedThisFrame_ = false;
}

void AssetThumbnailCache::Clear() {
	// 借りていたハンドルの記録を消す。GPUリソースの寿命は取得先が管理する。
	thumbnails_.clear();
}

//=============================================================================================//
// Getter / サムネイル

AssetThumbnail AssetThumbnailCache::GetThumbnail(const std::filesystem::path& path, const std::filesystem::path& resourcesRoot,
	const std::string& extension, IEditableWorld& scene, std::string& error) {
	// 取得済みのサムネイルは、フレーム内の準備上限に関係なく返す。
	const std::string key = AssetPathChange::Text(path);

	const std::unordered_map<std::string, AssetThumbnail>::const_iterator cached = thumbnails_.find(key);

	if (cached != thumbnails_.end()) {
		return cached->second;
	}

	// 未取得のAssetが並んでも、一度に多数のモデルや画像を読み込まない。
	if (wasThumbnailRequestedThisFrame_) {
		return {};
	}

	const std::string relativePath = AssetPathChange::Text(path.lexically_relative(resourcesRoot));

	const bool isModel = WorldObjects::IsSupportedModel(relativePath);

	if (!isModel) {
		// モデル以外でサムネイルを作るのは、対応する画像形式だけ。
		if (extension != ".png" && extension != ".jpg" && extension != ".jpeg" && extension != ".bmp") {
			return {};
		}

		// モデルの描画先にはWorld側の上限があり、画像の追加はここで制限する。
		if (thumbnails_.size() >= kCacheLimitForImages) {
			return {};
		}
	}

	wasThumbnailRequestedThisFrame_ = true;

	AssetThumbnail thumbnail;

	if (isModel) {
		// モデルはWorldが準備する128ピクセル四方の描画先を借りる。
		thumbnail.handle = scene.GetModelPreview(relativePath, error);

		thumbnail.width = 128;
		thumbnail.height = 128;
	} else {
		// 画像は元のテクスチャを使い、パネルで縦横比を保てるよう寸法も渡す。
		TextureManager* textureManager = TextureManager::GetInstance();

		if (textureManager->TryLoad(key, error)) {
			thumbnail.handle = textureManager->GetSrvHandleGPU(key).ptr;

			const DirectX::TexMetadata& metadata = textureManager->GetMetaData(key);

			thumbnail.width = static_cast<float>(metadata.width);
			thumbnail.height = static_cast<float>(metadata.height);
		}
	}

	// 読込失敗も覚え、同じ壊れたAssetの準備を毎フレーム繰り返さない。
	thumbnails_.emplace(key, thumbnail);

	return thumbnail;
}
