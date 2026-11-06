#pragma once

#include "Object3d.h"
#include "SceneRenderTarget.h"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class Model;
class Object3dCommon;
struct AssetPathChange;

// 3Dサムネイルの描画先とモデルをシーンごとに保持する。
class ModelPreviewRenderer {
public:

	//=============================================================================================//
	// 初期化・描画・終了

	// シーンと同じ描画基盤を借り、サムネイルの描画先は要求時に作る。
	void Initialize(Object3dCommon* object3dCommon);

	// 新しく作ったサムネイルを、シーンの描画が始まる前に一度だけ描く。
	void Draw();

	// GPUによる描画完了後、借用中のハンドルを含む全リソースを解放する。
	void Finalize();

	//=============================================================================================//
	// Getter / サムネイル

	// 準備済みのモデルの描画用ハンドルを返す。未登録なら0を返す。
	uint64_t GetTextureHandle(const std::string& path) const;

	bool CanCreatePreview() const;

	//=============================================================================================//
	// 描画先の準備・パスの更新

	// 読込済みのモデルを登録し、次のDrawで画像を描く。
	uint64_t CreatePreview(const std::string& path, Model& model);

	// Assetを移動しても、描画先をそのまま新しいパスから引けるようにする。
	void RemapAssetPaths(const AssetPathChange& change);

private:

	//=============================================================================================//
	// プレビューの描画データ

	struct Preview {
		SceneRenderTarget target;

		std::unique_ptr<Object3d> object;

		Matrix4x4 world{};

		bool wasRendered = false;
	};

	static constexpr std::size_t kMaxPreviews = 24;

	static constexpr uint32_t kPreviewSize = 128;

	Object3dCommon* object3dCommon_ = nullptr;

	std::unordered_map<std::string, std::unique_ptr<Preview>> previews_;

	// 移動前のハンドルがすでに描画へ渡っている可能性があるため、解放はFinalizeで行う。
	std::vector<std::unique_ptr<Preview>> retiredPreviews_;
};
