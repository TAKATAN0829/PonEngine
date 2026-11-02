#include "SceneViewPanel.h"
#include "../../EditorSelection.h"
#include "SceneViewDrawing.h"
#include "MathUtility.h"
#include "systems/WorldLighting.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cmath>
#include <numbers>

namespace {
	Vector3 CrossDirections(const Vector3& first, const Vector3& second) {
		return {first.y*second.z-first.z*second.y,first.z*second.x-first.x*second.z,first.x*second.y-first.y*second.x};
	}

	// Spotは実際の全開角・ワールド距離を使う。Directionalは照射方向だけを矢印にする。
	void DrawDirectedLight(const EditableObjectView& object, const Matrix4x4& world, const Vector3& direction,
		const EditorViewInfo& view, const ImVec2& origin) {
		const Vector3 position = {world.m[3][0],world.m[3][1],world.m[3][2]};
		Vector3 hint = {0,1,0};
		if (std::abs(direction.y) > 0.9f) {
			hint = {1,0,0};
		}
		const Vector3 right = MathUtility::Normalize(CrossDirections(hint,direction));
		const Vector3 up = CrossDirections(direction,right);
		ImDrawList& drawList = *ImGui::GetWindowDrawList();
		const ImU32 color = IM_COL32(255,210,100,220);
		if (object.type == WorldObjectType::DirectionalLight) {
			const Vector3 end = MathUtility::Add(position,MathUtility::Multiply(2,direction));
			SceneViewDrawing::DrawWorldLine(drawList,view,origin,position,end,color,2);
			const Vector3 head = MathUtility::Subtract(end,MathUtility::Multiply(0.35f,direction));
			SceneViewDrawing::DrawWorldLine(drawList,view,origin,end,MathUtility::Add(head,MathUtility::Multiply(0.2f,right)),color,2);
			SceneViewDrawing::DrawWorldLine(drawList,view,origin,end,MathUtility::Subtract(head,MathUtility::Multiply(0.2f,right)),color,2);
			return;
		}
		if (object.light == nullptr || object.lightRadius == nullptr) {
			return;
		}
		const Vector3 center = MathUtility::Add(position,MathUtility::Multiply(*object.lightRadius,direction));
		const float radius = *object.lightRadius*std::tan(object.light->outerAngle*std::numbers::pi_v<float>/360);
		for (size_t segment = 0; segment < 48; ++segment) {
			const float firstAngle = static_cast<float>(segment)*2*std::numbers::pi_v<float>/48;
			const float secondAngle = static_cast<float>(segment+1)*2*std::numbers::pi_v<float>/48;
			const Vector3 first = MathUtility::Add(center,MathUtility::Multiply(radius,MathUtility::Add(MathUtility::Multiply(std::cos(firstAngle),right),MathUtility::Multiply(std::sin(firstAngle),up))));
			const Vector3 second = MathUtility::Add(center,MathUtility::Multiply(radius,MathUtility::Add(MathUtility::Multiply(std::cos(secondAngle),right),MathUtility::Multiply(std::sin(secondAngle),up))));
			SceneViewDrawing::DrawWorldLine(drawList,view,origin,first,second,color,1.5f);
			if (segment%12 == 0) {
				SceneViewDrawing::DrawWorldLine(drawList,view,origin,position,first,color,1.5f);
			}
		}
	}
	void DrawCameraGuide(const CameraView& camera, const EditorViewInfo& view, const ImVec2& origin) {
		// 実カメラと同じ射影を逆変換し、FOV・Clip・親の回転を反映する。
		const Matrix4x4 inverse = MathUtility::Inverse(camera.viewProjection);
		Vector3 corners[8]{};
		for (size_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
			Vector3 corner = {-1,-1,0};
			if ((cornerIndex & 1) != 0) {
				corner.x = 1;
			}
			if ((cornerIndex & 2) != 0) {
				corner.y = 1;
			}
			if ((cornerIndex & 4) != 0) {
				corner.z = 1;
			}
			corners[cornerIndex] = MathUtility::Transform(corner,inverse);
		}
		for (size_t cornerIndex = 0; cornerIndex < 8; ++cornerIndex) {
			for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
				const size_t other = cornerIndex ^ (size_t{1} << axisIndex);
				if (cornerIndex < other) {
					SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(),view,origin,corners[cornerIndex],corners[other],IM_COL32(165,225,180,220),1.5f);
				}
			}
		}
	}

	Vector3 LightCirclePoint(const Vector3& center, float radius, size_t axisIndex, size_t segment) {
		const float angle = static_cast<float>(segment)*2*std::numbers::pi_v<float>/64;
		Vector3 offset = {std::cos(angle),std::sin(angle),0};
		if (axisIndex == 0) {
			offset = {0,std::cos(angle),std::sin(angle)};
		} else if (axisIndex == 1) {
			offset = {std::cos(angle),0,std::sin(angle)};
		}
		return MathUtility::Add(center,MathUtility::Multiply(radius,offset));
	}
}

void SceneViewPanel::DrawGuides(IEditableWorld& scene, const EditorSelection& selection, const EditorViewInfo& view, const ImVec2& origin) {
	if (!showGuides_) {
		return;
	}
	for (WorldObjectId identifier : selection.GetIds()) {
		const EditableObjectView object = FindObject(scene,identifier);
		if (!scene.IsObjectActive(identifier)) {
			continue;
		}
		if (object.type == WorldObjectType::SpotLight || object.type == WorldObjectType::DirectionalLight) {
			Matrix4x4 world{};
			Vector3 direction{};
			if (scene.GetObjectWorldMatrix(identifier,world)) {
				if (WorldLighting::GetDirection(world,direction)) {
					DrawDirectedLight(object,world,direction,view,origin);
				}
			}
			continue;
		}
		if (object.type == WorldObjectType::Camera) {
			CameraView camera{};
			if (scene.GetCameraView(identifier,view.viewportWidth/view.viewportHeight,camera)) {
				DrawCameraGuide(camera,view,origin);
			}
		} else if (object.type == WorldObjectType::PointLight) {
			if (object.lightRadius == nullptr) {
				continue;
			}
			// 描画側と同じワールド半径。Transformの拡縮は光の半径に掛けない。
			const Vector3 center = scene.GetObjectWorldPosition(identifier);
			for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
				for (size_t segment = 0; segment < 64; ++segment) {
					SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(),view,origin,LightCirclePoint(center,*object.lightRadius,axisIndex,segment),
						LightCirclePoint(center,*object.lightRadius,axisIndex,segment+1),IM_COL32(255,220,100,200),1.5f);
				}
			}
		}
	}
}
#endif
