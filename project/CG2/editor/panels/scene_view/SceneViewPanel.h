#pragma once

#include "IEditableWorld.h"

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

	void Draw(IEditableWorld& scene, EditorSelection& selection);

	//=============================================================================================//
	// Getter / 操作状態・画像サイズ

	bool IsManipulating() const;

	uint32_t GetWidth() const;

	uint32_t GetHeight() const;

	//=============================================================================================//
	// Setter / 表示画像・タブ

	void SetTexture(uint64_t textureHandle);

	void ShowScene();

	//=============================================================================================//
	// 操作の解除

	void ResetInteraction();

private:

	//=============================================================================================//
	// 型 / ギズモとドラッグ開始値

	enum class TransformTool { 
		Move, 
		Rotate,
		Scale 
	};

	// 選択ルートの初期値だけを保存し、親子を重複して動かさない。
	struct DragObject {
		WorldObjectId identifier;
		TransformData local;
		Vector3 position;
		Matrix4x4 world;
	};

	//=============================================================================================//
	// 更新処理 / ドラッグ

	// パネル内の座標を使い、開始位置からの移動量を求める。
	void Update(IEditableWorld& scene, const EditorViewInfo& viewInfo, const Vector2& mousePosition);

	//=============================================================================================//
	// Transform更新・復元

	// 位置を戻してからドラッグを解除する。
	void CancelDrag(IEditableWorld& scene);

	// 一括変換に失敗した場合は全対象を開始値へ戻し、部分的な操作を残さない。
	bool ApplyDragTransform(IEditableWorld& scene, const Matrix4x4& delta);

	//=============================================================================================//
	// ID検索・選択・Asset配置

	static EditableObjectView FindObject(IEditableWorld& scene, WorldObjectId objectId);

	//=============================================================================================//
	// 表示領域

	// 編集タブの内容だけを描き、GameViewの処理を混在させない。
	void DrawScene(IEditableWorld& scene, EditorSelection& selection);

#ifdef USE_IMGUI
	bool ResizeViewport(IEditableWorld& scene, const ImVec2& availableSize);

	//=============================================================================================//
	// 補助表示

	void DrawOverlay(IEditableWorld& scene, const EditorViewInfo& viewInfo, const ImVec2& origin);

	// 選択中のColliderだけを描き、GameViewへは表示しない。
	void DrawColliders(IEditableWorld& scene, const EditorSelection& selection, const EditorViewInfo& viewInfo, const ImVec2& origin);

	// カメラの近・遠平面と光源の影響球は、選択中だけSceneViewへ描く。
	void DrawGuides(IEditableWorld& scene, const EditorSelection& selection, const EditorViewInfo& viewInfo, const ImVec2& origin);

	//=============================================================================================//
	// ギズモ描画・操作開始

	// 最も近い一軸を確定してから、全選択ルートの開始値を保存する。
	bool BeginDrag(IEditableWorld& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
		const Vector2& mousePosition, const Vector2& screenOrigin, const Vector3& axisOrigin,
		const Vector3& axisDirection, float axisLength, size_t axisIndex);

	bool DrawGizmo(IEditableWorld& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
		const ImVec2& viewportOrigin, const Vector2& mousePosition, bool isImageHovered);

	//=============================================================================================//
	// 選択・配置

	bool AcceptModelDrop(IEditableWorld& scene, EditorSelection& selection, const ImVec2& viewportOrigin);

	void PickObject(IEditableWorld& scene, EditorSelection& selection, const EditorViewInfo& viewInfo, const Vector2& mousePosition);

#endif

private:

	//=============================================================================================//
	// ドラッグ状態

	std::string placementStatus_; // ドロップ失敗の理由。

	TransformTool tool_ = TransformTool::Move;

	std::vector<DragObject> dragObjects_;

	TransformData dragStartTransform_{};

	Matrix4x4 dragStartWorld_{};

	Vector2 dragStartMouse_{}; // 真横から見た回転リングの操作基準。

	float dragStartAngle_ = 0;

	size_t dragAxisIndex_ = 0; // 拡縮するローカル軸。

	float dragAxisLength_ = 1;

	bool isDragging_ = false;
	bool isGroupDrag_ = false; // 親子を選んでルートが一つになってもグループ操作として扱う。

	WorldObjectId draggedObject_ = kInvalidWorldObjectId;

	Vector3 dragStartPosition_{}; // キャンセルと移動量計算の基準。
	Vector3 dragAxis_{}; // ワールド座標での移動軸。

	float dragStartDistance_ = 0; // 操作開始時の軸上距離。

	//=============================================================================================//
	// 補助表示

	float snapDistance_ = 1; // Ctrl操作時に揃える間隔。
	float snapAngle_ = 15; // 回転は度数で指定し、適用時だけラジアンへ変換する。
	float snapScale_ = 0.1f; // 拡縮は開始値に対する倍率の刻み。

	bool isSnapEnabled_ = false; // 常時有効、またはCtrlを押している間にスナップする。
	bool shouldShowGrid_ = true;
	bool shouldShowGuides_ = true; // 選択中カメラ・ライトの補助線。

	//=============================================================================================//
	// 表示画像

	uint64_t textureHandle_ = 0; // SceneRenderTargetが所有するSRV。

	uint32_t width_ = 800;
	uint32_t height_ = 500;

	//=============================================================================================//
	// 表示タブ

	bool isGameView_ = false; // trueならゲーム用カメラの画像を表示する。
	bool shouldShowScene_ = false; // 次の描画でSceneタブへ切り替える。
};
