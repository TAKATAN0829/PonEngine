#pragma once

#include "IEditableScene.h"

// 選択対象の設定を借用して編集する。オブジェクトは所有しない。
class InspectorPanel {
public:

	//=============================================================================================//
	// 描画処理

	static void Draw(const EditableObjectView& object, bool* particleEditorOpen = nullptr);
};
