#pragma once

#include "IEditableWorld.h"
#include <string>

class EditorSelection;

// 選択対象の設定を借用して編集する。オブジェクトは所有しない。
class InspectorPanel {
public:

	//=============================================================================================//
	// 初期化処理

	static void Initialize();

	//=============================================================================================//
	// 描画処理

	static void Draw(const EditableObjectView& object, bool* particleEditorOpen = nullptr, IEditableWorld* scene = nullptr);

	// 選択はEditorから借用し、単体と複数選択の表示を同じクラスで切り替える。
	static void DrawSelection(IEditableWorld& scene, EditorSelection& selection, bool* particleEditorOpen = nullptr);

private:

	//=============================================================================================//
	// 複数選択

	// 共通Transformは差分、Activeと色は選択全体へ明示した値を反映する。
	static void DrawMultipleSelection(IEditableWorld& scene, const EditorSelection& selection);

	//=============================================================================================//
	// Material・Animation

	// モデル既定値と、シーンに保存する上書き設定を区別して編集する。
	static void DrawMaterials(const EditableObjectView& object, IEditableWorld& scene);

	static void DrawAnimation(const EditableObjectView& object, IEditableWorld& scene);

	//=============================================================================================//
	// Rigidbody

	// 質量・重力・接触応答を編集する。実行中の速度は保持しない。
	static void DrawBodyComponent(const EditableObjectView& object);

	//=============================================================================================//
	// Collider

	// BoxとSphereの形状・大きさ・Triggerを編集する。
	static void DrawColliderComponent(const EditableObjectView& object);

	//=============================================================================================//
	// Light

	// Point / Spot / Directionalの共通設定と角度を編集する。
	static void DrawLightComponent(const EditableObjectView& object);

	//=============================================================================================//
	// Motion

	// 回転・往復移動の設定と、右クリックによるComponentの追加を扱う。
	static void DrawMotionComponents(const EditableObjectView& object, IEditableWorld* scene);

private:

	//=============================================================================================//
	// 編集結果

	inline static std::string status_; // 読み込めない画像の理由。シーン切替時に消す。
};
