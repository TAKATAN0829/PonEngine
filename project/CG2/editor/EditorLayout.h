#pragma once

#include <string>
#include "IEditableScene.h"

// UIは選ばれた操作だけを返し、保存・読込の実行はEditorへ任せる。
enum class EditorMenuAction { None, NewScene, SaveScene, LoadScene };

// 初期ドッキングを構築し、その後の配置変更はImGuiの保存済み設定で復元する。
class EditorLayout {
public:

	//=============================================================================================//
	// 描画処理

	// シーン操作メニュー、作業用DockSpace、結果通知を描く。
	// メニューを開き、呼出元が同じ親領域へ再生操作を描けるようにする。
	EditorMenuAction Begin(bool sceneAvailable, bool playing, bool dirty, EditorToolVisibility& visibility);
	// DockSpaceと通知欄を描き、Beginで開いた親領域を閉じる。
	void Draw(const std::string& status);

private:

	//=============================================================================================//
	// メンバ変数 / レイアウト再構築

	bool resetRequested_ = false; // Viewメニューから初期配置への復帰を要求する。
};
