#pragma once

#include "IEditableScene.h"

// 参照の寿命に依存しないよう、選択対象をIDだけで管理する。
class EditorSelection {
public:

	//=============================================================================================//
	// Getter / 選択ID

	SceneObjectId Get() const { return selected_; }

	//=============================================================================================//
	// Setter / 選択ID

	void Select(SceneObjectId id) { selected_ = id; }

	//=============================================================================================//
	// メンバ関数 / 選択解除

	void Clear() { selected_ = kInvalidSceneObjectId; }

private:

	//=============================================================================================//
	// メンバ変数 / 選択ID

	SceneObjectId selected_ = kInvalidSceneObjectId; // 未選択は0。
};
