#include "SceneViewPanel.h"
#include "SceneViewDrawing.h"
#ifdef USE_IMGUI
namespace {
	// ワールド原点のXZ平面へ補助線を描く。
	void DrawGrid(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin) {
		for (int gridIndex = -10; gridIndex <= 10; ++gridIndex) {
			const float gridPosition = static_cast<float>(gridIndex);
			SceneViewDrawing::DrawWorldLine(drawList, viewInfo, viewportOrigin, {gridPosition, 0, -10}, {gridPosition, 0, 10},
				IM_COL32(170, 170, 170, 70), 1);
			SceneViewDrawing::DrawWorldLine(drawList, viewInfo, viewportOrigin, {-10, 0, gridPosition}, {10, 0, gridPosition},
				IM_COL32(170, 170, 170, 70), 1);
		}
	}

	// メッシュがない対象も識別できるよう、種類別のマーカーを描く。
	void DrawObjectMarkers(IEditableWorld& scene, ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin) {
		// メッシュがない対象の位置も常時表示し、Scene内で見つけられるようにする。
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);
			if (object.type == WorldObjectType::Model) {
				continue;
			}
			if (object.transform == nullptr) {
				continue;
			}
			if (!scene.IsObjectActive(object.id)) {
				continue;
			}
			Vector2 screenPosition{};
			if (!SceneViewMath::Project(scene.GetObjectWorldPosition(object.id), viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight, screenPosition)) {
				continue;
			}
			const ImVec2 position = SceneViewDrawing::ToDrawPosition(screenPosition, viewportOrigin);
			ImU32 markerColor = IM_COL32(190,200,220,255);
			const char* markerName = "Empty";
			if (object.type == WorldObjectType::PointLight) {
				markerColor = IM_COL32(255,220,90,255);
				markerName = "Light";
			}
			if (object.type == WorldObjectType::SpotLight) {
				markerColor = IM_COL32(255,190,100,255);
				markerName = "Spot Light";
			}
			if (object.type == WorldObjectType::DirectionalLight) {
				markerColor = IM_COL32(255,235,140,255);
				markerName = "Directional Light";
			}
			if (object.type == WorldObjectType::Particle) {
				markerColor = IM_COL32(100,210,255,255);
				markerName = "Particle";
			}
			if (object.type == WorldObjectType::Camera) {
				markerColor = IM_COL32(170,220,180,255);
				markerName = "Camera";
			}
			drawList.AddCircle(position, 7, markerColor, 12, 2);
			drawList.AddLine({position.x-10,position.y}, {position.x+10,position.y}, markerColor);
			drawList.AddLine({position.x,position.y-10}, {position.x,position.y+10}, markerColor);
			drawList.AddText({position.x+12,position.y-7}, markerColor, markerName);
		}
	}
}

// グリッドとメッシュを持たない対象のマーカーを重ねる。
void SceneViewPanel::DrawOverlay(IEditableWorld& scene, const EditorViewInfo& viewInfo, const ImVec2& origin) {
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	if (showGrid_) {
		DrawGrid(drawList,viewInfo,origin);
	}
	DrawObjectMarkers(scene,drawList,viewInfo,origin);
}
#endif
