#pragma once
#ifdef USE_IMGUI
#include "IEditableWorld.h"
#include "SceneViewMath.h"
#include "imgui/imgui.h"

// SceneView内だけの共通描画。投影計算はGPU非依存のSceneViewMathへ任せる。
namespace SceneViewDrawing {
	// カメラの投影結果を、ImGuiが使う画面上の位置へ合わせる。
	ImVec2 ToDrawPosition(const Vector2& position, const ImVec2& viewportOrigin);

	// 奥行範囲内にある線だけを背景へ重ねる。パネルより手前には描画しない。
	void DrawWorldLine(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin,
		const Vector3& start, const Vector3& end, ImU32 color, float thickness);
}

#endif
