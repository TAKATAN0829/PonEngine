#pragma once

#include "../EditorPanel.h"
#include "IEditableWorld.h"
#include <string>

class EditorSelection;

// 選択対象の設定を借用して編集する。オブジェクトは所有しない。
class InspectorPanel : public EditorPanel {
public:
	InspectorPanel() : EditorPanel("Inspector") {}

	//=============================================================================================//
	// 初期化処理

	void Initialize();

	//=============================================================================================//
	// 描画処理

	void Draw(const EditableObjectView& object, IWorldObjects* objects = nullptr,
		IModelAssetAccess* models = nullptr, ITextureAssetAccess* textures = nullptr,
		bool* isParticleEditorOpen = nullptr);

	// 選択はEditorから借用し、単体と複数選択の表示を同じクラスで切り替える。
	void DrawSelection(IWorldObjects& objects, IModelAssetAccess& models, ITextureAssetAccess& textures,
		EditorSelection& selection,
		bool* isParticleEditorOpen = nullptr);

private:

	//=============================================================================================//
	// 複数選択

	// 共通Transformは差分、Activeと色は選択全体へ明示した値を反映する。
	void DrawMultipleSelection(IWorldObjects& objects, const EditorSelection& selection);

	//=============================================================================================//
	// Material・Animation

	// モデル既定値と、シーンに保存する上書き設定を区別して編集する。
	void DrawMaterials(const EditableObjectView& object, IModelAssetAccess& models, ITextureAssetAccess& textures);

	static void DrawAnimation(const EditableObjectView& object, IModelAssetAccess& models);

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
	static void DrawMotionComponents(const EditableObjectView& object, IWorldObjects* objects);

private:

	//=============================================================================================//
	// 編集結果

	std::string status_; // 読み込めない画像の理由。シーン切替時に消す。
};
