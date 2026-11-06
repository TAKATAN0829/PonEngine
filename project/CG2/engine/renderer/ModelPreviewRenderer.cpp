#include "ModelPreviewRenderer.h"
#include "AssetPathChange.h"
#include "Model.h"
#include "Object3dCommon.h"
#include "SceneViewMath.h"
#include <algorithm>
#include <utility>

//=============================================================================================//
// 初期化・描画・終了

void ModelPreviewRenderer::Initialize(Object3dCommon* object3dCommon) {
	// 描画用の共通設定はシーンより長く生存する描画基盤から借りる。
	object3dCommon_ = object3dCommon;
}

void ModelPreviewRenderer::Draw() {
	for (const std::pair<const std::string, std::unique_ptr<Preview>>& entry : previews_) {
		Preview& preview = *entry.second;

		// 角度は固定なので、一度描いた画像はそのまま次のフレームでも使う。
		if (preview.wasRendered) {
			continue;
		}

		// 原点に寄せたモデルを、正方形の画像に専用のカメラで映す。
		const Matrix4x4 cameraWorld = MathUtility::MakeAffineMatrix({1, 1, 1}, {}, {0, 0, -3});
		const Matrix4x4 projection = MathUtility::MakePerspectiveFovMatrix(0.785398163f, 1, 0.1f, 100);

		preview.object->Update(MathUtility::Multiply(MathUtility::Inverse(cameraWorld), projection), preview.world);

		preview.target.Begin();

		object3dCommon_->DrawSettingCommon();

		preview.object->Draw();

		// 描画結果をImGuiがサンプリングできる状態へ戻す。
		preview.target.End();

		preview.wasRendered = true;
	}
}

void ModelPreviewRenderer::Finalize() {
	// 通常のキャッシュと移動時に退避した描画先を、GPU完了後にまとめて解放する。
	for (const std::pair<const std::string, std::unique_ptr<Preview>>& entry : previews_) {
		entry.second->target.Finalize();
	}

	previews_.clear();

	for (const std::unique_ptr<Preview>& preview : retiredPreviews_) {
		preview->target.Finalize();
	}

	retiredPreviews_.clear();

	object3dCommon_ = nullptr;
}

//=============================================================================================//
// Getter / サムネイル

uint64_t ModelPreviewRenderer::GetTextureHandle(const std::string& path) const {
	// 登録済みなら同じ描画先を返し、モデルの読み込みやリソース作成を省く。
	const std::unordered_map<std::string, std::unique_ptr<Preview>>::const_iterator preview = previews_.find(path);

	if (preview == previews_.end()) {
		return 0;
	}

	return preview->second->target.GetTextureHandle();
}

bool ModelPreviewRenderer::CanCreatePreview() const {
	// 描画先の作成数を制限して、Projectを巡回しても確保量が増え続けないようにする。
	if (previews_.size() < kMaxPreviews) {
		return true;
	}

	return false;
}

//=============================================================================================//
// 描画先の準備・パスの更新

uint64_t ModelPreviewRenderer::CreatePreview(const std::string& path, Model& model) {
	// モデルが用意できたときだけ、専用の正方形の描画先を確保する。
	std::unique_ptr<Preview> preview = std::make_unique<Preview>();

	preview->target.Initialize(object3dCommon_->GetDxCommon(), object3dCommon_->GetSrvManager());

	preview->target.Resize(kPreviewSize, kPreviewSize);

	preview->object = std::make_unique<Object3d>();

	preview->object->Initialize(object3dCommon_);

	// ゲームのライトに左右されず、形状が分かるように描画設定をそろえる。
	preview->object->SetModel(&model);

	preview->object->SetTexture("resources/white1x1.png");

	preview->object->SetEnableLighting(false);

	preview->object->SetMaterials({});

	// モデルの原点や大きさによらず、外接範囲の中心を画面中央に合わせる。
	const AABB bounds = SceneViewMath::TransformBounds(model.GetLocalBounds(), model.GetRootNode().localMatrix);

	const float diameter = (std::max)({bounds.max.x - bounds.min.x, bounds.max.y - bounds.min.y, bounds.max.z - bounds.min.z, 0.001f});
	const float scale = 1.2f / diameter;

	const Vector3 center = MathUtility::Multiply(0.5f, MathUtility::Add(bounds.min, bounds.max));

	preview->world = MathUtility::MakeAffineMatrix({scale, scale, scale}, {}, MathUtility::Multiply(-scale, center));

	// 少し斜めから見せ、奥行きが分かるようにする。
	preview->world = MathUtility::Multiply(preview->world, MathUtility::MakeAffineMatrix({1, 1, 1}, {0.2f, 0.4f, 0}, {}));

	const uint64_t handle = preview->target.GetTextureHandle();

	previews_[path] = std::move(preview);

	return handle;
}

void ModelPreviewRenderer::RemapAssetPaths(const AssetPathChange& change) {
	// 一覧を走査してからキーを書き換え、走査中に要素の位置を変えない。
	std::vector<std::string> changedPaths;

	for (const std::pair<const std::string, std::unique_ptr<Preview>>& entry : previews_) {
		if (change.RemapRelative(entry.first) != entry.first) {
			changedPaths.push_back(entry.first);
		}
	}

	for (const std::string& path : changedPaths) {
		const std::string destination = change.RemapRelative(path);

		std::unordered_map<std::string, std::unique_ptr<Preview>>::node_type node = previews_.extract(path);

		// 古い移動先の画像が今のフレームで使われている場合に備え、即時解放を避ける。
		if (previews_.contains(destination)) {
			retiredPreviews_.push_back(std::move(previews_.at(destination)));

			previews_.erase(destination);
		}

		// キーだけを替え、描画先とハンドルはそのまま引き継ぐ。
		node.key() = destination;

		previews_.insert(std::move(node));
	}
}
