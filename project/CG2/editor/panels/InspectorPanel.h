#pragma once

#include "IEditableScene.h"

// 選択対象の設定を借用して編集する。オブジェクトは所有しない。
class InspectorPanel {
public:
	//=============================================================================================//
	// 初期化処理

	static void Initialize() { status_.clear(); }

	//=============================================================================================//
	// 描画処理

	static void Draw(const EditableObjectView& object, bool* particleEditorOpen = nullptr, IEditableScene* scene = nullptr);

private:
	//=============================================================================================//
	// メンバ関数 / アセットComponent編集

	// 空白の右クリックから追加し、各Componentの見出しから削除する。
	static void DrawMotionComponents(const EditableObjectView& object, IEditableScene* scene);
	static void DrawColliderComponent(const EditableObjectView& object);
	static void DrawBodyComponent(const EditableObjectView& object);
	static void DrawMaterials(const EditableObjectView& object, IEditableScene& scene);
	static void DrawAnimation(const EditableObjectView& object, IEditableScene& scene);

private:
	//=============================================================================================//
	// メンバ変数 / 編集結果

	inline static std::string status_; // 読み込めない画像の理由。シーン切替時に消す。
};
