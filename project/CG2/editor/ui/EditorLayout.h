#pragma once

#include <string>
#include "IEditableWorld.h"

// UIは選ばれた操作だけを返し、保存・読込の実行はEditorへ任せる。
enum class EditorMenuAction { None, NewScene, SaveScene, SaveSceneAs, LoadScene };

// 初期ドッキングを構築し、その後の配置変更はImGuiの保存済み設定で復元する。
class EditorLayout {
public:

	//=============================================================================================//
	// 描画処理

	// シーン操作メニュー、作業用DockSpace、結果通知を描く。
	// メニューを開き、呼出元が同じ親領域へ再生操作を描けるようにする。
	EditorMenuAction Begin(bool isSceneAvailable, bool isPlaying, bool hasUnsavedChanges, EditorToolVisibility& visibility);

	// DockSpaceと通知欄を描き、Beginで開いた親領域を閉じる。
	void Draw(const std::string& status);

private:

	//=============================================================================================//
	// レイアウト再構築

	bool isResetRequested_ = false; // Viewメニューから初期配置への復帰を要求する。
};
