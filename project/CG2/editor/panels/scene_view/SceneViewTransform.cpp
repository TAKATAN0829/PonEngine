#include "SceneViewPanel.h"
#include "SceneViewMath.h"
#include "MathUtility.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <algorithm>
#include <cmath>
#endif

// ドラッグ開始値からTransformを更新し、失敗・キャンセル時は全対象を復元する。
void SceneViewPanel::Update(IEditableWorld& scene, const EditorViewInfo& viewInfo, const Vector2& mousePosition) {
#ifdef USE_IMGUI
	const EditableObjectView object = FindObject(scene, draggedObject_);

	if (object.transform == nullptr) {
		ResetInteraction();
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		CancelDrag(scene);
		return;
	}
	if (tool_ == TransformTool::Rotate) {
		Vector2 center{};
		if (SceneViewMath::Project(dragStartPosition_,viewInfo.viewProjection,viewInfo.viewportWidth,viewInfo.viewportHeight,center)) {
			float angle = std::remainder(std::atan2(mousePosition.y-center.y,mousePosition.x-center.x)-dragStartAngle_,6.283185307f);
			const Vector3 toCamera = MathUtility::Subtract(viewInfo.cameraPosition,dragStartPosition_);
			if (MathUtility::Length(toCamera) > 0.001f) {
				if (std::abs(MathUtility::Dot(MathUtility::Normalize(toCamera),dragAxis_)) < 0.2f) {
					angle = (mousePosition.x-dragStartMouse_.x-mousePosition.y+dragStartMouse_.y) * 0.01f;
				} else if (MathUtility::Dot(toCamera,dragAxis_) < 0) {
					// 画面Yは下向き。軸を裏側から見る場合は角度の符号を反転する。
					angle = -angle;
				}
			}
			if (snapEnabled_ || ImGui::GetIO().KeyCtrl) {
				const float increment = snapAngle_ * 0.0174532925f;
				angle = std::round(angle / increment) * increment;
			}
			Matrix4x4 rotation = MathUtility::MakeRotateXMatrix(angle);
			if (dragAxis_.y > 0.5f) {
				rotation = MathUtility::MakeRotateYMatrix(angle);
			}
			if (dragAxis_.z > 0.5f) {
				rotation = MathUtility::MakeRotateZMatrix(angle);
			}
			Matrix4x4 delta = MathUtility::MakeTranslateMatrix(MathUtility::Multiply(-1.0f, dragStartPosition_));
			delta = MathUtility::Multiply(MathUtility::Multiply(delta, rotation), MathUtility::MakeTranslateMatrix(dragStartPosition_));
			ApplyDragTransform(scene, delta);
		}
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ResetInteraction();
		}
		return;
	}
	Vector3 rayOrigin{};
	Vector3 rayDirection{};
	float axisDistance = 0;
	if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight,
		rayOrigin, rayDirection)) {
		if (SceneViewMath::GetAxisDistance(rayOrigin, rayDirection, dragStartPosition_, dragAxis_, axisDistance)) {
			float movement = axisDistance - dragStartDistance_;
			if (tool_ == TransformTool::Move) {
				if (snapEnabled_ || ImGui::GetIO().KeyCtrl) {
					movement = std::round(movement / snapDistance_) * snapDistance_;
				}
			}
			if (tool_ == TransformTool::Scale) {
				float factor = 1.0f + movement / dragAxisLength_;
				if (snapEnabled_ || ImGui::GetIO().KeyCtrl) {
					factor = 1.0f + std::round((factor-1.0f)/snapScale_)*snapScale_;
				}
				factor = (std::clamp)(factor,0.01f,100.0f);
				if (groupDrag_) {
					// ワールドの任意軸へ拡縮する。TRSで表せないせん断は全体を拒否する。
					Matrix4x4 scale = MathUtility::MakeIdentity4x4();
					const float axisValues[] = {dragAxis_.x, dragAxis_.y, dragAxis_.z};
					for (size_t row = 0; row < 3; ++row) {
						for (size_t column = 0; column < 3; ++column) {
							scale.m[row][column] += (factor - 1) * axisValues[row] * axisValues[column];
						}
					}
					Matrix4x4 delta = MathUtility::MakeTranslateMatrix(MathUtility::Multiply(-1.0f, dragStartPosition_));
					delta = MathUtility::Multiply(MathUtility::Multiply(delta, scale), MathUtility::MakeTranslateMatrix(dragStartPosition_));
					ApplyDragTransform(scene, delta);
				} else {
					object.transform->scale = dragStartTransform_.scale;
					if (dragAxisIndex_ == 0) {
						object.transform->scale.x *= factor;
					}
					if (dragAxisIndex_ == 1) {
						object.transform->scale.y *= factor;
					}
					if (dragAxisIndex_ == 2) {
						object.transform->scale.z *= factor;
					}
				}
			} else {
				for (const DragObject& target : dragObjects_) {
					if (!scene.SetObjectWorldPosition(target.identifier, MathUtility::Add(target.position, MathUtility::Multiply(movement, dragAxis_)))) {
						placementStatus_ = "親のTransformを逆変換できないため、一括移動を中止しました。";
						CancelDrag(scene);
						return;
					}
				}
			}
		}
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		ResetInteraction();
	} else {
		// ギズモ操作中のUndoなどで、編集中の対象が切り替わることを防ぐ。
		ImGui::SetNextFrameWantCaptureKeyboard(true);
	}
#else
	(void)scene;
	(void)viewInfo;
	(void)mousePosition;
#endif
}

void SceneViewPanel::CancelDrag(IEditableWorld& scene) {
	for (const DragObject& target : dragObjects_) {
		const EditableObjectView object = FindObject(scene, target.identifier);
		if (object.transform != nullptr) {
			*object.transform = target.local;
		}
	}
	ResetInteraction();
}

bool SceneViewPanel::ApplyDragTransform(IEditableWorld& scene, const Matrix4x4& delta) {
	for (const DragObject& target : dragObjects_) {
		if (!scene.SetObjectWorldTransform(target.identifier, MathUtility::Multiply(target.world, delta))) {
			for (const DragObject& restore : dragObjects_) {
				const EditableObjectView object = FindObject(scene, restore.identifier);
				if (object.transform != nullptr) {
					*object.transform = restore.local;
				}
			}
			placementStatus_ = "親の非均等拡縮などでせん断が発生するため、この一括変換は適用できません。";
			return false;
		}
	}
	placementStatus_.clear();
	return true;
}
