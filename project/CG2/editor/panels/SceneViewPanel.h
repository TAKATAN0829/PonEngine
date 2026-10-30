#pragma once

#include "IEditableScene.h"

class EditorSelection;
struct ImVec2;

// Scene画像へ選択枠と移動ギズモを重ねる。描画リソースは借用する。
class SceneViewPanel {
public:

	//=============================================================================================//
	// 初期化処理

	// 切替前のドラッグを終了し、以前の対象IDを参照しないようにする。
	void Initialize();

	//=============================================================================================//
	// 描画処理

	void Draw(IEditableScene& scene, EditorSelection& selection);

	//=============================================================================================//
	// Getter / 操作状態・画像サイズ

	bool IsManipulating() const { return dragging_; }
	uint32_t GetWidth() const { return width_; }
	uint32_t GetHeight() const { return height_; }

	//=============================================================================================//
	// Setter / 表示画像・タブ

	void SetTexture(uint64_t textureHandle) { textureHandle_ = textureHandle; }
	void ShowScene() { forceScene_ = true; }

	//=============================================================================================//
	// メンバ関数 / 操作の解除

	void ResetInteraction();

private:

	//=============================================================================================//
	// 更新処理 / ドラッグ

	// パネル内の座標を使い、開始位置からの移動量を求める。
	void Update(IEditableScene& scene, const EditorViewInfo& viewInfo, const Vector2& mousePosition);

	//=============================================================================================//
	// Getter / ID検索

	// 位置を戻してからドラッグを解除する。
	void CancelDrag(IEditableScene& scene);
	static EditableObjectView FindObject(IEditableScene& scene, SceneObjectId objectId);

#ifdef USE_IMGUI
	//=============================================================================================//
	// メンバ関数 / サイズ・ギズモ・選択

	bool ResizeViewport(IEditableScene& scene, const ImVec2& availableSize);
	bool DrawGizmo(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
		const ImVec2& viewportOrigin, const Vector2& mousePosition, bool imageHovered);
	void PickObject(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo, const Vector2& mousePosition);
#endif

private:

	//=============================================================================================//
	// メンバ変数 / ドラッグ状態

	bool dragging_ = false;
	SceneObjectId draggedObject_ = kInvalidSceneObjectId;
	Vector3 dragStartLocalPosition_{}; // Esc・表示サイズ変更時に戻すローカル位置。
	Vector3 dragStartPosition_{}; // キャンセルと移動量計算の基準。
	Vector3 dragAxis_{}; // ワールド座標での移動軸。
	float dragStartDistance_ = 0; // 操作開始時の軸上距離。

	//=============================================================================================//
	// メンバ変数 / 補助表示

	float snapDistance_ = 1; // Ctrl操作時に揃える間隔。
	bool showGrid_ = true;

	//=============================================================================================//
	// メンバ変数 / 表示画像

	uint64_t textureHandle_ = 0; // SceneRenderTargetが所有するSRV。
	uint32_t width_ = 800;
	uint32_t height_ = 500;

	//=============================================================================================//
	// メンバ変数 / 表示タブ

	bool gameView_ = false; // trueならゲーム用カメラの画像を表示する。
	bool forceScene_ = false; // 次の描画でSceneタブへ切り替える。
};
