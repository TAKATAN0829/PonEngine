#pragma once

#include "IEditableScene.h"

class EditorSelection;

// Projectに読み込み済みアセット、Consoleに実際のログを表示する。
class UtilityPanels {
public:

	//=============================================================================================//
	// 描画処理

	void Draw(IEditableScene& scene, const EditorSelection& selection, EditorToolVisibility& visibility);
};
