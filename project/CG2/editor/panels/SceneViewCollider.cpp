#include "SceneViewPanel.h"
#include "../EditorSelection.h"
#include "SceneCollision.h"
#include "SceneViewMath.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cmath>
#include <numbers>

namespace {
	void DrawColliderLine(const EditorViewInfo& view, const ImVec2& origin, const Vector3& first, const Vector3& second, ImU32 color) {
		Vector2 firstScreen{};
		Vector2 secondScreen{};
		if (!SceneViewMath::Project(first,view.viewProjection,view.viewportWidth,view.viewportHeight,firstScreen)) {
			return;
		}
		if (!SceneViewMath::Project(second,view.viewProjection,view.viewportWidth,view.viewportHeight,secondScreen)) {
			return;
		}
		ImGui::GetWindowDrawList()->AddLine({origin.x+firstScreen.x,origin.y+firstScreen.y},{origin.x+secondScreen.x,origin.y+secondScreen.y},color,2);
	}
	Vector3 SpherePoint(const WorldCollider& sphere, size_t axis, size_t segment) {
		const float angle = static_cast<float>(segment) * 2 * std::numbers::pi_v<float> / 48;
		Vector3 offset{};
		if (axis == 0) {
			offset = {0,std::cos(angle),std::sin(angle)};
		}
		if (axis == 1) {
			offset = {std::cos(angle),0,std::sin(angle)};
		}
		if (axis == 2) {
			offset = {std::cos(angle),std::sin(angle),0};
		}
		return MathUtility::Add(sphere.center,MathUtility::Multiply(sphere.radius,offset));
	}
}

void SceneViewPanel::DrawColliders(IEditableScene& scene, const EditorSelection& selection, const EditorViewInfo& view, const ImVec2& origin) {
	for (SceneObjectId identifier : selection.GetIds()) {
		const SceneObjectData* object = scene.GetObjectDataById(identifier);
		if (object == nullptr) {
			continue;
		}
		if (!object->collider.present) {
			continue;
		}
		Matrix4x4 world{};
		WorldCollider shape;
		if (!scene.GetObjectWorldMatrix(identifier,world)) {
			continue;
		}
		if (!SceneCollision::BuildWorld(object->collider,world,shape)) {
			continue;
		}
		ImU32 color = IM_COL32(80,240,145,255);
		if (scene.IsColliderTouching(identifier)) {
			color = IM_COL32(255,100,90,255);
		}
		if (!object->collider.enabled) {
			color = IM_COL32(150,150,150,180);
		} else if (!scene.IsObjectActive(identifier)) {
			color = IM_COL32(150,150,150,180);
		}
		if (shape.shape == ColliderShape::Box) {
			for (size_t corner = 0; corner < 8; ++corner) {
				for (size_t axis = 0; axis < 3; ++axis) {
					const size_t other = corner ^ (size_t{1} << axis);
					if (corner < other) {
						DrawColliderLine(view,origin,SceneCollision::GetCorner(shape,corner),SceneCollision::GetCorner(shape,other),color);
					}
				}
			}
		} else {
			for (size_t axis = 0; axis < 3; ++axis) {
				for (size_t segment = 0; segment < 48; ++segment) {
					DrawColliderLine(view,origin,SpherePoint(shape,axis,segment),SpherePoint(shape,axis,segment+1),color);
				}
			}
		}
	}
}
#endif
