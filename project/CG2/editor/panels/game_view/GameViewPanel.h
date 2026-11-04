#pragma once

#include "IEditableWorld.h"

// GameViewは画像表示だけを担当し、選択・ギズモ・編集カメラの状態は持たない。
namespace GameViewPanel {
	//=============================================================================================//
	// 描画処理

	// 選択中のタブの寸法を共通の管理元へ返す。画像ハンドルの所有権は取得しない。
	void Draw(IEditableWorld& scene, uint64_t textureHandle, uint32_t& width, uint32_t& height);
}
