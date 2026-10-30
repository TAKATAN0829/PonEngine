#include "SceneViewPanel.h"
#include "../EditorSelection.h"
#include "SceneViewMath.h"
#include "MathUtility.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cmath>
#include <algorithm>
#include <limits>

namespace {
	//=============================================================================================//
	// 定数 / 移動軸・選択枠

	constexpr Vector3 kAxisDirections[3] = {{1,0,0}, {0,1,0}, {0,0,1}};
	constexpr ImU32 kAxisColors[3] = {IM_COL32(245,75,75,255), IM_COL32(90,230,100,255), IM_COL32(90,150,255,255)};
	constexpr const char* kAxisNames[3] = {"X", "Y", "Z"};
	constexpr size_t kBoundsEdges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
	// カメラの投影結果を、ImGuiが使う画面上の位置へ合わせる。
	ImVec2 ToDrawPosition(const Vector2& position, const ImVec2& viewportOrigin) {
		return {position.x + viewportOrigin.x, position.y + viewportOrigin.y};
	}

	// 奥行範囲内にある線だけを背景へ重ねる。パネルより手前には描画しない。
	void DrawWorldLine(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin,
		const Vector3& start, const Vector3& end, ImU32 color, float thickness) {
		Vector2 screenStart{};
		Vector2 screenEnd{};
		if (SceneViewMath::Project(start, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight, screenStart) &&
			SceneViewMath::Project(end, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight, screenEnd)) {
			drawList.AddLine(ToDrawPosition(screenStart, viewportOrigin), ToDrawPosition(screenEnd, viewportOrigin), color, thickness);
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
			DrawWorldLine(drawList, viewInfo, viewportOrigin, corners[kBoundsEdges[edgeIndex][0]], corners[kBoundsEdges[edgeIndex][1]], IM_COL32(255, 210, 60, 255), 1.5f);
		}
	}

	// ワールド原点のXZ平面へ補助線を描く。
	void DrawGrid(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin) {
		for (int gridIndex = -10; gridIndex <= 10; ++gridIndex) {
			const float gridPosition = static_cast<float>(gridIndex);
			DrawWorldLine(drawList, viewInfo, viewportOrigin, {gridPosition, 0, -10}, {gridPosition, 0, 10},
				IM_COL32(170, 170, 170, 70), 1);
			DrawWorldLine(drawList, viewInfo, viewportOrigin, {-10, 0, gridPosition}, {10, 0, gridPosition},
				IM_COL32(170, 170, 170, 70), 1);
		}
	}

	// メッシュがない対象も識別できるよう、種類別のマーカーを描く。
	void DrawObjectMarkers(IEditableScene& scene, ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin) {
		// メッシュがない対象の位置も常時表示し、Scene内で見つけられるようにする。
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);
			if (object.type == SceneObjectType::Model) {
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
			const ImVec2 position = ToDrawPosition(screenPosition, viewportOrigin);
			ImU32 markerColor = IM_COL32(190,200,220,255);
			const char* markerName = "Empty";
			if (object.type == SceneObjectType::PointLight) {
				markerColor = IM_COL32(255,220,90,255);
				markerName = "Light";
			}
			if (object.type == SceneObjectType::Particle) {
				markerColor = IM_COL32(100,210,255,255);
				markerName = "Particle";
			}
			if (object.type == SceneObjectType::Camera) {
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
#endif

// 借用参照は操作のたびに取得し直し、一覧の追加・削除後に古いポインタを使わない。
EditableObjectView SceneViewPanel::FindObject(IEditableScene& scene, SceneObjectId objectId) {
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == objectId) {
			return object;
		}
	}
	return {};
}

//=============================================================================================//
// 初期化処理

void SceneViewPanel::Initialize() {
	ResetInteraction();
	dragStartPosition_ = {};
	dragStartLocalPosition_ = {};
	dragAxis_ = {};
	dragStartDistance_ = 0;
	forceScene_ = false;
}

// 読込・シーン切替・再生切替時に、前のシーンのドラッグ状態を引き継がない。
void SceneViewPanel::ResetInteraction() {
	dragging_ = false;
	draggedObject_ = kInvalidSceneObjectId;
}

// 毎回開始位置を基準に計算し、フレームごとの加算誤差やスナップのずれを防ぐ。
void SceneViewPanel::Update(IEditableScene& scene, const EditorViewInfo& viewInfo, const Vector2& mousePosition) {
#ifdef USE_IMGUI
	const EditableObjectView object = FindObject(scene, draggedObject_);

	if (object.transform == nullptr) {
		ResetInteraction();
		return;
	}

	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		object.transform->translate = dragStartLocalPosition_;
		ResetInteraction();
		return;
	}
	Vector3 rayOrigin{};
	Vector3 rayDirection{};
	float axisDistance = 0;
	if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight,
		rayOrigin, rayDirection)) {
		if (SceneViewMath::GetAxisDistance(rayOrigin, rayDirection, dragStartPosition_, dragAxis_, axisDistance)) {
			float movement = axisDistance - dragStartDistance_;
			if (ImGui::GetIO().KeyCtrl) {
				if (snapDistance_ > 0) {
					movement = std::round(movement / snapDistance_) * snapDistance_;
				}
			}
			scene.SetObjectWorldPosition(object.id, MathUtility::Add(dragStartPosition_, MathUtility::Multiply(movement, dragAxis_)));
		}
	}
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		ResetInteraction();
	} else {
		// 移動中のWASDでカメラが動き、操作軸がずれることを防ぐ。
		ImGui::SetNextFrameWantCaptureKeyboard(true);
	}
#else
	(void)scene;
	(void)viewInfo;
	(void)mousePosition;
#endif
}

// パネル操作中のクリックは拾わず、背景のモデル選択とワールド軸の移動だけを扱う。
void SceneViewPanel::Draw(IEditableScene& scene, EditorSelection& selection) {
#ifdef USE_IMGUI
	scene.SetEditorCameraInput(false);
	if (!ImGui::Begin("Scene Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		ResetInteraction();
		ImGui::End();
		return;
	}
	if (ImGui::BeginTabBar("SceneViewTabs")) {
		ImGuiTabItemFlags sceneFlags = ImGuiTabItemFlags_None;
		if (forceScene_) {
			sceneFlags = ImGuiTabItemFlags_SetSelected;
		}
		if (ImGui::BeginTabItem("SceneView", nullptr, sceneFlags)) {
			gameView_ = false;
			ImGui::EndTabItem();
		}
		if (ImGui::BeginTabItem("GameView")) {
			gameView_ = true;
			ImGui::EndTabItem();
		}
		ImGui::EndTabBar();
		forceScene_ = false;
	}
	EditableObjectView selectedObject = FindObject(scene, selection.Get());
	if (!gameView_) {
		ImGui::Checkbox("Grid", &showGrid_);
		ImGui::SameLine();
		ImGui::SetNextItemWidth(95);
		ImGui::DragFloat("Snap", &snapDistance_, 0.1f, 0.1f, 100, "%.1f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::SameLine();
		ImGui::BeginDisabled(selectedObject.transform == nullptr || dragging_);
		if (ImGui::Button("注視 F")) {
			scene.FocusEditorCamera(scene.GetObjectWorldPosition(selectedObject.id));
		}
		ImGui::EndDisabled();
	} else {
		SceneCameraView gameCamera{};
		if (scene.GetGameCameraView(static_cast<float>(width_) / static_cast<float>(height_), gameCamera)) {
			ImGui::TextUnformatted("Main Camera / Playで動作確認");
		} else {
			ImGui::TextDisabled("メインカメラ未指定・無効：既定の視点を使用中");
		}
	}
	if (!ResizeViewport(scene, ImGui::GetContentRegionAvail())) {
		ImGui::End();
		return;
	}
	const ImVec2 viewportOrigin = ImGui::GetCursorScreenPos();
	// 画像・クリック・ギズモはすべて同じ矩形を使う。
	ImGui::Image(ImTextureRef(static_cast<ImTextureID>(textureHandle_)),
		{static_cast<float>(width_), static_cast<float>(height_)});
	const bool imageHovered = ImGui::IsItemHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
	if (gameView_) {
		ImGui::End();
		return;
	}
	if (selectedObject.transform != nullptr) {
		if (imageHovered && !dragging_) {
			if (!ImGui::GetIO().WantTextInput && !ImGui::IsAnyItemActive()) {
				if (!ImGui::GetIO().KeyCtrl) {
					if (ImGui::IsKeyPressed(ImGuiKey_F, false)) {
						scene.FocusEditorCamera(scene.GetObjectWorldPosition(selectedObject.id));
					}
				}
			}
		}
	}
	EditorViewInfo viewInfo;
	if (!scene.GetEditorViewInfo(viewInfo)) {
		ResetInteraction();
		ImGui::End();
		return;
	}
	const Vector2 mousePosition = {ImGui::GetIO().MousePos.x - viewportOrigin.x, ImGui::GetIO().MousePos.y - viewportOrigin.y};
	if (dragging_) {
		Update(scene, viewInfo, mousePosition);
	}

	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	drawList.PushClipRect(viewportOrigin, {viewportOrigin.x + viewInfo.viewportWidth, viewportOrigin.y + viewInfo.viewportHeight}, true);
	if (showGrid_) {
		DrawGrid(drawList, viewInfo, viewportOrigin);
	}
	DrawObjectMarkers(scene, drawList, viewInfo, viewportOrigin);
	const bool clickedGizmo = DrawGizmo(scene, selection, viewInfo, viewportOrigin, mousePosition, imageHovered);
	drawList.PopClipRect();

	if (!dragging_ && !clickedGizmo) {
		if (imageHovered) {
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				PickObject(scene, selection, viewInfo, mousePosition);
			}
		}
	}
	scene.SetEditorCameraInput(imageHovered && !dragging_ && !ImGui::IsAnyItemActive());
	ImGui::End();
#else
	(void)scene;
	(void)selection;
#endif
}

#ifdef USE_IMGUI
//=============================================================================================//
// メンバ関数 / 表示サイズ更新

bool SceneViewPanel::ResizeViewport(IEditableScene& scene, const ImVec2& availableSize) {
	if (textureHandle_ == 0) {
		ResetInteraction();
		return false;
	}
	if (availableSize.x < 64 || availableSize.y < 64) {
		ResetInteraction();
		return false;
	}
	const uint32_t nextWidth = static_cast<uint32_t>((std::min)(availableSize.x, 4096.0f));
	const uint32_t nextHeight = static_cast<uint32_t>((std::min)(availableSize.y, 4096.0f));
	if (dragging_) {
		if (gameView_) {
			CancelDrag(scene);
		}
		else if (width_ != nextWidth || height_ != nextHeight) {
			CancelDrag(scene);
		}
	}
	width_ = nextWidth;
	height_ = nextHeight;
	scene.SetEditorViewport(width_, height_);
	scene.SetEditorGameView(gameView_);
	return true;
}

//=============================================================================================//
// メンバ関数 / 移動軸描画

bool SceneViewPanel::DrawGizmo(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const ImVec2& viewportOrigin, const Vector2& mousePosition, bool imageHovered) {
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	bool clickedGizmo = false;
	const EditableObjectView selectedObject = FindObject(scene, selection.Get());
	if (selectedObject.transform != nullptr) {
		// 範囲はモデルの形状から求め、原点付近を固定半径で選ぶ方法は使わない。
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			if (scene.GetEditableObject(objectIndex).id == selection.Get()) {
				AABB bounds;
				if (scene.GetEditableObjectBounds(objectIndex, bounds)) {
					DrawSelectionBounds(drawList, viewInfo, viewportOrigin, bounds);
				}
				break;
			}
		}

		const Vector3 axisOrigin = scene.GetObjectWorldPosition(selectedObject.id);
		float axisLength = MathUtility::Length(MathUtility::Subtract(axisOrigin, viewInfo.cameraPosition)) * 0.12f;
		if (axisLength < 0.5f) {
			axisLength = 0.5f;
		}
		Vector2 screenOrigin{};
		if (SceneViewMath::Project(axisOrigin, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight, screenOrigin)) {
			for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
				const Vector3 axisEnd = MathUtility::Add(axisOrigin, MathUtility::Multiply(axisLength, kAxisDirections[axisIndex]));
				Vector2 screenEnd{};
				if (!SceneViewMath::Project(axisEnd, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight, screenEnd)) {
					continue;
				}
				if (std::hypot(screenEnd.x - screenOrigin.x, screenEnd.y - screenOrigin.y) < 12) {
					continue;
				}
				const bool hovered = imageHovered &&
					SceneViewMath::DistanceToSegment(mousePosition, screenOrigin, screenEnd) < 7;
				ImU32 color = kAxisColors[axisIndex];
				if (hovered) {
					color = IM_COL32(255, 230, 90, 255);
				}
				else if (dragging_) {
					if (MathUtility::Dot(dragAxis_, kAxisDirections[axisIndex]) > 0.99f) {
						color = IM_COL32(255, 230, 90, 255);
					}
				}
				drawList.AddLine(ToDrawPosition(screenOrigin, viewportOrigin), ToDrawPosition(screenEnd, viewportOrigin), color, 3);
				drawList.AddCircleFilled(ToDrawPosition(screenEnd, viewportOrigin), 5, color);
				drawList.AddText(ToDrawPosition(screenEnd, viewportOrigin), color, kAxisNames[axisIndex]);
				if (hovered) {
					if (!dragging_ && !clickedGizmo) {
						if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
							Vector3 rayOrigin{};
							Vector3 rayDirection{};
							if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight,
								rayOrigin, rayDirection)) {
								if (SceneViewMath::GetAxisDistance(rayOrigin, rayDirection,
									axisOrigin, kAxisDirections[axisIndex], dragStartDistance_)) {
									dragging_ = true;
									draggedObject_ = selectedObject.id;
									dragStartPosition_ = axisOrigin;
									dragStartLocalPosition_ = selectedObject.transform->translate;
									dragAxis_ = kAxisDirections[axisIndex];
									clickedGizmo = true;
								}
							}
						}
					}
				}
			}
		}
	}
	return clickedGizmo;
}

//=============================================================================================//
// メンバ関数 / レイによる選択

void SceneViewPanel::PickObject(IEditableScene& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const Vector2& mousePosition) {
	Vector3 rayOrigin{};
	Vector3 rayDirection{};
	float rayLength = 0;
	if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight,
		rayOrigin, rayDirection, &rayLength)) {
		SceneObjectId closestObject = kInvalidSceneObjectId;
		float closestDistance = (std::numeric_limits<float>::max)();
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			AABB bounds;
			float hitDistance = 0;
			if (!scene.GetEditableObjectBounds(objectIndex, bounds)) {
				continue;
			}
			if (SceneViewMath::IntersectBounds(rayOrigin, rayDirection, bounds, hitDistance)) {
				if (hitDistance <= rayLength && hitDistance < closestDistance) {
					closestDistance = hitDistance;
					closestObject = scene.GetEditableObject(objectIndex).id;
				}
			}
		}
		selection.Select(closestObject);
	}
}
#endif

// 操作開始位置へ戻してから移動状態を解除する。
void SceneViewPanel::CancelDrag(IEditableScene& scene) {
	const EditableObjectView object = FindObject(scene, draggedObject_);
	if (object.transform != nullptr) {
		object.transform->translate = dragStartLocalPosition_;
	}
	ResetInteraction();
}
