#include "SceneViewDrawing.h"

#ifdef USE_IMGUI

//=============================================================================================//
// 共通処理

// カメラの投影結果を、ImGuiが使う画面上の位置へ合わせる。
ImVec2 SceneViewDrawing::ToDrawPosition(const Vector2& position, const ImVec2& viewportOrigin) {
	// 投影した座標へScene画像の画面位置を加える。
	return {position.x + viewportOrigin.x, position.y + viewportOrigin.y};
}

// 奥行範囲内にある線だけを背景へ重ねる。パネルより手前には描画しない。
void SceneViewDrawing::DrawWorldLine(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin,
		const Vector3& start, const Vector3& end, ImU32 color, float thickness) {
	// 表示可能な線分だけを画面へ投影して描く。
	Vector2 screenStart{};
	Vector2 screenEnd{};

	// 線分を画像内の座標へ投影し、画像の左上を足して描画リストに渡す。
	if (SceneViewMath::ProjectLine(start, end, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight, screenStart, screenEnd)) {
		drawList.AddLine(ToDrawPosition(screenStart, viewportOrigin), ToDrawPosition(screenEnd, viewportOrigin), color, thickness);
	}
}

#endif
