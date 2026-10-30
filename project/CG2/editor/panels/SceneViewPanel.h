#pragma once

#include "IEditableScene.h"

class EditorSelection;
struct ImVec2;

// Scene画像へ選択枠と移動・回転・拡縮ギズモを重ねる。描画リソースは借用する。
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
	// 一括変換に失敗した場合は全対象を開始値へ戻し、部分的な操作を残さない。
	bool ApplyDragTransform(IEditableScene& scene, const Matrix4x4& delta);
	static EditableObjectView FindObject(IEditableScene& scene, SceneObjectId objectId);

#ifdef USE_IMGUI
	//=============================================================================================//
	// メンバ関数 / サイズ・ギズモ・選択

	bool AcceptModelDrop(IEditableScene& scene, EditorSelection& selection, const ImVec2& viewportOrigin);
	bool ResizeViewport(IEditableScene& scene, const ImVec2& availableSize);
	bool DrawGizmo(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
		const ImVec2& viewportOrigin, const Vector2& mousePosition, bool imageHovered);
	void PickObject(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo, const Vector2& mousePosition);
#endif

private:

	//=============================================================================================//
	// メンバ変数 / ドラッグ状態

	std::string placementStatus_; // ドロップ失敗の理由。
	enum class TransformTool { Move, Rotate, Scale };
	TransformTool tool_ = TransformTool::Move;
	// 選択ルートの初期値だけを保存し、親子を重複して動かさない。
	struct DragObject {
		SceneObjectId identifier = 0;
		TransformData local{};
		Vector3 position{};
		Matrix4x4 world{};
	};
	std::vector<DragObject> dragObjects_;
	TransformData dragStartTransform_{};
	Matrix4x4 dragStartWorld_{};
	Vector2 dragStartMouse_{}; // 真横から見た回転リングの操作基準。
	float dragStartAngle_ = 0;
	size_t dragAxisIndex_ = 0; // 拡縮するローカル軸。
	float dragAxisLength_ = 1;
	bool dragging_ = false;
	bool groupDrag_ = false; // 親子を選んでルートが一つになってもグループ操作として扱う。
	SceneObjectId draggedObject_ = kInvalidSceneObjectId;
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
