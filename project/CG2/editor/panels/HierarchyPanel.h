#pragma once

class ModelCollection;
class EditorSelection;

class HierarchyPanel {
public:

	//===========================================//
	// 描画処理
	//===========================================//

	static void Draw(ModelCollection* models, EditorSelection& selection);
};
