#include "SceneViewPanel.h"
#include "SceneViewDrawing.h"
#include "../EditorSelection.h"
#include "MathUtility.h"
#include "components/SceneLightSettings.h"
#ifdef USE_IMGUI
#include <algorithm>
#include <cmath>

namespace {
	constexpr Vector3 kAxisDirections[3] = {{1,0,0},{0,1,0},{0,0,1}};
	constexpr ImU32 kAxisColors[3] = {IM_COL32(245,75,75,255),IM_COL32(90,230,100,255),IM_COL32(90,150,255,255)};
	constexpr const char* kAxisNames[3] = {"X","Y","Z"};
	constexpr size_t kBoundsEdges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
	// 選択軸に直交する二軸を使い、回転リング上の一点を求める。
	Vector3 GetRotationRingPoint(const Vector3& origin, float radius, size_t axisIndex, int segment) {
		const float angle = static_cast<float>(segment) * 6.283185307f / 48;
		const Vector3 horizontal = MathUtility::Multiply(std::cos(angle), kAxisDirections[(axisIndex + 1) % 3]);
		const Vector3 vertical = MathUtility::Multiply(std::sin(angle), kAxisDirections[(axisIndex + 2) % 3]);
		return MathUtility::Add(origin, MathUtility::Multiply(radius, MathUtility::Add(horizontal, vertical)));
	}

	// 判定と描画を分け、重なったリングでは最も近い一軸だけを強調する。
	float GetRotationRingDistance(const EditorViewInfo& viewInfo, const Vector3& origin,
		float radius, size_t axisIndex, const Vector2& mousePosition) {
		float nearest = 1000000;
		for (int segment = 0; segment < 48; ++segment) {
			Vector2 first{};
			Vector2 second{};
			if (SceneViewMath::ProjectLine(GetRotationRingPoint(origin,radius,axisIndex,segment),
				GetRotationRingPoint(origin,radius,axisIndex,segment+1),viewInfo.viewProjection,
				viewInfo.viewportWidth,viewInfo.viewportHeight,first,second)) {
				nearest = (std::min)(nearest,SceneViewMath::DistanceToSegment(mousePosition,first,second));
			}
		}
		return nearest;
	}

	void DrawRotationRing(const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin,
		const Vector3& origin, float radius, size_t axisIndex, ImU32 color, float thickness) {
		for (int segment = 0; segment < 48; ++segment) {
			SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(),viewInfo,viewportOrigin,
				GetRotationRingPoint(origin,radius,axisIndex,segment),
				GetRotationRingPoint(origin,radius,axisIndex,segment+1),color,thickness);
		}
	}

	// モデルのワールドAABBの十二辺を表示し、クリック判定に使う範囲を確認できるようにする。
	void DrawSelectionBounds(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin, const AABB& bounds) {
		Vector3 corners[8]{};
		for (size_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {

			corners[cornerIndex] = bounds.min;

			if ((cornerIndex & 1) != 0) {
				corners[cornerIndex].x = bounds.max.x;
			}

			if ((cornerIndex & 2) != 0) {
				corners[cornerIndex].y = bounds.max.y;
			}

			if ((cornerIndex & 4) != 0) {
				corners[cornerIndex].z = bounds.max.z;
			}
		}

		for (size_t edgeIndex = 0; edgeIndex < 12; ++edgeIndex) {
			SceneViewDrawing::DrawWorldLine(drawList, viewInfo, viewportOrigin, corners[kBoundsEdges[edgeIndex][0]], corners[kBoundsEdges[edgeIndex][1]], IM_COL32(255, 210, 60, 255), 1.5f);
		}
	}
}

// ギズモの描画・ヒット判定と、ドラッグ開始値の取得を担当する。
bool SceneViewPanel::DrawGizmo(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const ImVec2& viewportOrigin, const Vector2& mousePosition, bool imageHovered) {
	const EditableObjectView object = FindObject(scene,selection.Get());
	if (object.transform == nullptr) {
		return false;
	}
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	// 操作できない種類・ツールの組合せでも、選択枠は維持する。
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		if (selection.Contains(scene.GetEditableObject(objectIndex).id)) {
			AABB bounds{};
			if (scene.GetEditableObjectBounds(objectIndex,bounds)) {
				DrawSelectionBounds(drawList,viewInfo,viewportOrigin,bounds);
			}
		}
	}
	if (tool_ != TransformTool::Move) {
		if (object.type == SceneObjectType::PointLight || object.type == SceneObjectType::Particle) {
			return false;
		}
	}
	if (tool_ == TransformTool::Scale) {
		if (object.type == SceneObjectType::Camera || SceneLightTypes::IsLight(object.type)) {
			return false;
		}
	}
	const Vector3 axisOrigin = scene.GetObjectWorldPosition(object.id);
	const float axisLength = SceneViewMath::GetHandleLength(axisOrigin,viewInfo.viewProjection,
		viewInfo.viewportWidth,viewInfo.viewportHeight,80);
	Vector2 screenOrigin{};
	if (axisLength <= 0 || !SceneViewMath::Project(axisOrigin,viewInfo.viewProjection,viewInfo.viewportWidth,viewInfo.viewportHeight,screenOrigin)) {
		return false;
	}
	// この呼出内だけに画面位置を保持し、シーンの借用参照は保存しない。
	struct AxisHandle {
		Vector3 direction{};
		Vector2 end{};
		bool visible = false;
	};
	AxisHandle handles[3]{};
	size_t hoveredAxis = 3;
	float nearest = 9;
	Matrix4x4 world{};
	if (!scene.GetObjectWorldMatrix(object.id,world)) {
		return false;
	}
	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		handles[axisIndex].direction = kAxisDirections[axisIndex];
		if (tool_ == TransformTool::Scale) {
			handles[axisIndex].direction = {world.m[axisIndex][0],world.m[axisIndex][1],world.m[axisIndex][2]};
			if (MathUtility::Length(handles[axisIndex].direction) < 0.000001f) {
				continue;
			}
			handles[axisIndex].direction = MathUtility::Normalize(handles[axisIndex].direction);
		}
		const Vector3 axisEnd = MathUtility::Add(axisOrigin,MathUtility::Multiply(axisLength,handles[axisIndex].direction));
		handles[axisIndex].visible = SceneViewMath::Project(axisEnd,viewInfo.viewProjection,
			viewInfo.viewportWidth,viewInfo.viewportHeight,handles[axisIndex].end);
		float distance = 1000000;
		if (tool_ == TransformTool::Rotate) {
			handles[axisIndex].visible = true;
			distance = GetRotationRingDistance(viewInfo,axisOrigin,axisLength,axisIndex,mousePosition);
		} else if (handles[axisIndex].visible) {
			if (std::hypot(handles[axisIndex].end.x-screenOrigin.x,handles[axisIndex].end.y-screenOrigin.y) < 12) {
				handles[axisIndex].visible = false;
				continue;
			}
			distance = SceneViewMath::DistanceToSegment(mousePosition,screenOrigin,handles[axisIndex].end);
		}
		if (imageHovered && distance < nearest) {
			nearest = distance;
			hoveredAxis = axisIndex;
		}
	}
	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		if (!handles[axisIndex].visible) {
			continue;
		}
		bool highlighted = hoveredAxis == axisIndex;
		if (dragging_) {
			highlighted = dragAxisIndex_ == axisIndex;
		}
		ImU32 color = kAxisColors[axisIndex];
		float thickness = 2;
		if (highlighted) {
			color = IM_COL32(255,230,90,255);
			thickness = 4;
		}
		if (tool_ == TransformTool::Rotate) {
			DrawRotationRing(viewInfo,viewportOrigin,axisOrigin,axisLength,axisIndex,color,thickness);
			continue;
		}
		const ImVec2 start = SceneViewDrawing::ToDrawPosition(screenOrigin,viewportOrigin);
		const ImVec2 end = SceneViewDrawing::ToDrawPosition(handles[axisIndex].end,viewportOrigin);
		drawList.AddLine(start,end,color,thickness);
		if (tool_ == TransformTool::Scale) {
			drawList.AddRectFilled({end.x-5,end.y-5},{end.x+5,end.y+5},color);
		} else {
			const float length = std::hypot(end.x-start.x,end.y-start.y);
			const ImVec2 direction = {(end.x-start.x)/length,(end.y-start.y)/length};
			drawList.AddTriangleFilled(end,{end.x-direction.x*13-direction.y*5,end.y-direction.y*13+direction.x*5},
				{end.x-direction.x*13+direction.y*5,end.y-direction.y*13-direction.x*5},color);
		}
		drawList.AddText({end.x+7,end.y-7},color,kAxisNames[axisIndex]);
	}
	if (dragging_ || hoveredAxis >= 3) {
		return false;
	}
	ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return false;
	}
	return BeginDrag(scene,selection,viewInfo,mousePosition,screenOrigin,axisOrigin,
		handles[hoveredAxis].direction,axisLength,hoveredAxis);
}

bool SceneViewPanel::BeginDrag(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const Vector2& mousePosition, const Vector2& screenOrigin, const Vector3& axisOrigin,
	const Vector3& axisDirection, float axisLength, size_t axisIndex) {
	Vector3 rayOrigin{};
	Vector3 rayDirection{};
	if (!SceneViewMath::CreateRay(mousePosition,viewInfo.viewProjection,viewInfo.viewportWidth,
		viewInfo.viewportHeight,rayOrigin,rayDirection)) {
		return false;
	}
	if (tool_ != TransformTool::Rotate) {
		if (!SceneViewMath::GetAxisDistance(rayOrigin,rayDirection,axisOrigin,axisDirection,dragStartDistance_)) {
			return false;
		}
	}
	dragObjects_.clear();
	for (SceneObjectId identifier : selection.GetRoots(scene)) {
		const EditableObjectView target = FindObject(scene,identifier);
		if (target.transform == nullptr) {
			continue;
		}
		Matrix4x4 world{};
		if (!scene.GetObjectWorldMatrix(identifier,world)) {
			dragObjects_.clear();
			return false;
		}
		dragObjects_.push_back({identifier,*target.transform,scene.GetObjectWorldPosition(identifier),world});
	}
	if (dragObjects_.empty()) {
		return false;
	}
	const EditableObjectView selected = FindObject(scene,selection.Get());
	groupDrag_ = selection.GetIds().size() > 1;
	dragging_ = true;
	draggedObject_ = selected.id;
	dragStartPosition_ = axisOrigin;
	dragStartTransform_ = *selected.transform;
	scene.GetObjectWorldMatrix(selected.id,dragStartWorld_);
	dragStartMouse_ = mousePosition;
	dragStartAngle_ = std::atan2(mousePosition.y-screenOrigin.y,mousePosition.x-screenOrigin.x);
	dragAxisLength_ = axisLength;
	dragAxis_ = axisDirection;
	dragAxisIndex_ = axisIndex;
	placementStatus_.clear();
	return true;
}
#endif
