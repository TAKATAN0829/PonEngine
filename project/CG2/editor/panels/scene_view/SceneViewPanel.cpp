#include "SceneViewPanel.h"
#include "prefab/PrefabOperations.h"
#include "../game_view/GameViewPanel.h"
#include "FrameProfiler.h"
#include "../../state/EditorSelection.h"
#include <algorithm>
#include "MathUtility.h"
#include "settings/LightSettings.h"
#include <cmath>
#include "SceneViewMath.h"
#include "../../assets/AssetDragPayload.h"
#include <limits>
#include "systems/WorldCollision.h"
#include <numbers>
#include "systems/WorldLighting.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "SceneViewDrawing.h"
#endif

//=============================================================================================//
// 実装内補助関数（クラス外・ヘッダーには公開しない）


#ifdef USE_IMGUI
namespace {
	constexpr Vector3 kAxisDirections[3] = {{1,0,0},{0,1,0},{0,0,1}};

	constexpr ImU32 kAxisColors[3] = {IM_COL32(245,75,75,255),IM_COL32(90,230,100,255),IM_COL32(90,150,255,255)};

	constexpr const char* kAxisNames[3] = {"X","Y","Z"};

	constexpr size_t kBoundsEdges[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};

	// 選択軸に直交する二軸を使い、回転リング上の一点を求める。
	Vector3 GetRotationRingPoint(const Vector3& origin, float radius, size_t axisIndex, int segment) {
		// 回転軸と角度から、円周上の点を求める。
		const float angle = static_cast<float>(segment) * 6.283185307f / 48;

		const Vector3 horizontal = MathUtility::Multiply(std::cos(angle), kAxisDirections[(axisIndex + 1) % 3]);
		const Vector3 vertical = MathUtility::Multiply(std::sin(angle), kAxisDirections[(axisIndex + 2) % 3]);

		return MathUtility::Add(origin, MathUtility::Multiply(radius, MathUtility::Add(horizontal, vertical)));
	}

	// 判定と描画を分け、重なったリングでは最も近い一軸だけを強調する。
	float GetRotationRingDistance(const EditorViewInfo& viewInfo, const Vector3& origin,
		float radius, size_t axisIndex, const Vector2& mousePosition) {
		// 円周を線分で近似し、マウスから最も近い距離を求める。
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
		// 回転ギズモの円周を線分に分けて描く。
		for (int segment = 0; segment < 48; ++segment) {
			// 描画リストを取得。
			SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(),viewInfo,viewportOrigin,
				GetRotationRingPoint(origin,radius,axisIndex,segment),
				GetRotationRingPoint(origin,radius,axisIndex,segment+1),color,thickness);
		}
	}

	// モデルのワールドAABBの十二辺を表示し、クリック判定に使う範囲を確認できるようにする。
	void DrawSelectionBounds(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin, const AABB& bounds) {
		// 選択対象の範囲を画面へ投影し、枠線を描く。
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

#endif

#ifdef USE_IMGUI
namespace {
	// ワールド原点のXZ平面へ補助線を描く。
	void DrawGrid(ImDrawList& drawList, const EditorViewInfo& viewInfo, const ImVec2& viewportOrigin) {
		// 一定間隔のワールド線を画面へ投影し、床の目盛りを描く。
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

			// AddCircleを処理。
			drawList.AddCircle(position, 7, markerColor, 12, 2);
			// 線を描画。
			drawList.AddLine({position.x-10,position.y}, {position.x+10,position.y}, markerColor);
			// 線を描画。
			drawList.AddLine({position.x,position.y-10}, {position.x,position.y+10}, markerColor);
			// 文字列を描画。
			drawList.AddText({position.x+12,position.y-7}, markerColor, markerName);
		}
	}
}

#endif

#ifdef USE_IMGUI
namespace {
	void DrawColliderLine(const EditorViewInfo& view, const ImVec2& origin, const Vector3& first, const Vector3& second, ImU32 color) {
		// Colliderの線分を画面へ投影して描く。
		Vector2 firstScreen{};
		Vector2 secondScreen{};

		if (!SceneViewMath::Project(first,view.viewProjection,view.viewportWidth,view.viewportHeight,firstScreen)) {
			return;
		}

		if (!SceneViewMath::Project(second,view.viewProjection,view.viewportWidth,view.viewportHeight,secondScreen)) {
			return;
		}

		// 描画リストを取得。
		ImGui::GetWindowDrawList()->AddLine({origin.x+firstScreen.x,origin.y+firstScreen.y},{origin.x+secondScreen.x,origin.y+secondScreen.y},color,2);
	}

	Vector3 SpherePoint(const WorldCollider& sphere, size_t axis, size_t segment) {
		// 球面上の角度を、Colliderのワールド座標へ変換する。
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

#endif

#ifdef USE_IMGUI
namespace {
	Vector3 CrossDirections(const Vector3& first, const Vector3& second) {
		// 二つの方向ベクトルから、両方に垂直な方向を求める。
		return {first.y*second.z-first.z*second.y,first.z*second.x-first.x*second.z,first.x*second.y-first.y*second.x};
	}

	// Spotは実際の全開角・ワールド距離を使う。Directionalは照射方向だけを矢印にする。
	void DrawDirectedLight(const EditableObjectView& object, const Matrix4x4& world, const Vector3& direction,
		const EditorViewInfo& view, const ImVec2& origin) {
		// ライトの方向と種類に応じた補助形状を描く。
		const Vector3 position = {world.m[3][0],world.m[3][1],world.m[3][2]};

		Vector3 hint = {0,1,0};

		if (std::abs(direction.y) > 0.9f) {
			hint = {1,0,0};
		}

		const Vector3 right = MathUtility::Normalize(CrossDirections(hint,direction));
		const Vector3 up = CrossDirections(direction,right);

		// 描画リストを取得。
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
					// 描画リストを取得。
					SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(),view,origin,corners[cornerIndex],corners[other],IM_COL32(165,225,180,220),1.5f);
				}
			}
		}
	}

	Vector3 LightCirclePoint(const Vector3& center, float radius, size_t axisIndex, size_t segment) {
		// ライトの軸に垂直な円周上の点を求める。
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

#endif

//=============================================================================================//
// 初期化処理


// タブ・画像領域・操作の呼出し順を管理する。個々の描画・操作は同じcpp内の機能別の区切りへまとめる。
void SceneViewPanel::Initialize() {
	// 表示サイズとギズモの操作状態を初期化する。
	ResetInteraction();

	dragStartPosition_ = {};

	dragAxis_ = {};

	dragStartDistance_ = 0;

	shouldShowScene_ = false;
}

//=============================================================================================//
// 描画処理


// 共通の描画先とタブ切替を管理し、表示内容は各タブの実装へ任せる。
void SceneViewPanel::Draw(IEditableWorld& scene, EditorSelection& selection) {
	// Scene／Gameのタブを描き、画像上の入力を選択や操作へ渡す。
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::SceneView);

#ifdef USE_IMGUI
	scene.SetEditorCameraInput(false);

	// 「Scene Viewport」ウィンドウを開始。
	if (!ImGui::Begin("Scene Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse)) {
		ResetInteraction();

		// ウィンドウを終了。
		ImGui::End();

		return;
	}

	// 表示タブを切り替え、Scene編集とGame実行画面を同じ領域で使う。
	// 「SceneViewTabs」タブ一覧を開始。
	if (ImGui::BeginTabBar("SceneViewTabs")) {
		ImGuiTabItemFlags sceneFlags = ImGuiTabItemFlags_None;

		if (shouldShowScene_) {
			sceneFlags = ImGuiTabItemFlags_SetSelected;
		}

		// 「SceneView」タブ項目を開始。
		if (ImGui::BeginTabItem("SceneView", nullptr, sceneFlags)) {
			isGameView_ = false;

			// タブ項目を終了。
			ImGui::EndTabItem();
		}

		// 「GameView」タブ項目を開始。
		if (ImGui::BeginTabItem("GameView")) {
			isGameView_ = true;

			// タブ項目を終了。
			ImGui::EndTabItem();
		}

		// タブ一覧を終了。
		ImGui::EndTabBar();

		shouldShowScene_ = false;
	}

	if (isGameView_) {
		if (isDragging_) {
			CancelDrag(scene);
		}

		GameViewPanel::Draw(scene,textureHandle_,width_,height_);
	} else {
		DrawScene(scene,selection);
	}

	// ウィンドウを終了。
	ImGui::End();

#else
	(void)scene;

	(void)selection;

#endif
}

//=============================================================================================//
// Getter / 操作状態・画像サイズ

bool SceneViewPanel::IsManipulating() const {
	// ギズモで配置を変更している途中か確認する。
	if (isDragging_) {
		return true;
	}

	return false;
}

uint32_t SceneViewPanel::GetWidth() const {
	// Scene画像の幅を取得する。
	return width_;
}

uint32_t SceneViewPanel::GetHeight() const {
	// Scene画像の高さを取得する。
	return height_;
}

//=============================================================================================//
// Setter / 表示画像・タブ

void SceneViewPanel::SetTexture(uint64_t textureHandle) {
	// 指定画像を読み込み、描画時のテクスチャ参照を更新する。
	textureHandle_ = textureHandle;
}

void SceneViewPanel::ShowScene() {
	// 次の描画でSceneViewのタブを選択するよう要求する。
	shouldShowScene_ = true;
}

//=============================================================================================//
// 操作の解除


void SceneViewPanel::ResetInteraction() {
	// ギズモの操作状態と未確定の要求を解除する。
	isDragging_ = false;

	draggedObject_ = kInvalidWorldObjectId;

	dragObjects_.clear();
}

//=============================================================================================//
// 更新処理 / ドラッグ


// ドラッグ開始値からTransformを更新し、失敗・キャンセル時は全対象を復元する。
void SceneViewPanel::Update(IEditableWorld& scene, const EditorViewInfo& viewInfo, const Vector2& mousePosition) {
#ifdef USE_IMGUI
	// ドラッグ開始時のIDで対象を引き直し、途中で消えていたら操作を解除する。
	const EditableObjectView object = FindObject(scene, draggedObject_);

	if (object.transform == nullptr) {
		ResetInteraction();

		return;
	}

	// キー入力を確認。
	if (ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
		CancelDrag(scene);

		return;
	}

	if (tool_ == TransformTool::Rotate) {
		Vector2 center{};

		if (SceneViewMath::Project(dragStartPosition_,viewInfo.viewProjection,viewInfo.viewportWidth,viewInfo.viewportHeight,center)) {
			// 画面に映した回転中心を基準に、ドラッグ開始からの角度差を求める。
			float angle = std::remainder(std::atan2(mousePosition.y-center.y,mousePosition.x-center.x)-dragStartAngle_,6.283185307f);

			const Vector3 toCamera = MathUtility::Subtract(viewInfo.cameraPosition,dragStartPosition_);

			if (MathUtility::Length(toCamera) > 0.001f) {
				if (std::abs(MathUtility::Dot(MathUtility::Normalize(toCamera),dragAxis_)) < 0.2f) {
					// 回転リングを横から見ると角度を取りづらいため、マウスの移動量を使う。
					angle = (mousePosition.x-dragStartMouse_.x-mousePosition.y+dragStartMouse_.y) * 0.01f;
				} else if (MathUtility::Dot(toCamera,dragAxis_) < 0) {
					// 画面Yは下向き。軸を裏側から見る場合は角度の符号を反転する。
					angle = -angle;
				}
			}

			// 入力状態を取得。
			if (isSnapEnabled_ || ImGui::GetIO().KeyCtrl) {
				// 設定された度数をラジアンに換算し、最も近い刻みへ丸める。
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

			// 回転中心を一度原点へ移して回し、その後で元の位置へ戻す。
			Matrix4x4 delta = MathUtility::MakeTranslateMatrix(MathUtility::Multiply(-1.0f, dragStartPosition_));

			delta = MathUtility::Multiply(MathUtility::Multiply(delta, rotation), MathUtility::MakeTranslateMatrix(dragStartPosition_));

			ApplyDragTransform(scene, delta);
		}

		// マウス押下を確認。
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			ResetInteraction();
		}

		return;
	}

	Vector3 rayOrigin{};
	Vector3 rayDirection{};

	float axisDistance = 0;

	// 画面上のマウス位置をワールドのレイに変換し、操作軸に沿った移動量を求める。
	if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight,
		rayOrigin, rayDirection)) {
		if (SceneViewMath::GetAxisDistance(rayOrigin, rayDirection, dragStartPosition_, dragAxis_, axisDistance)) {
			float movement = axisDistance - dragStartDistance_;

			if (tool_ == TransformTool::Move) {
				// 入力状態を取得。
				if (isSnapEnabled_ || ImGui::GetIO().KeyCtrl) {
					movement = std::round(movement / snapDistance_) * snapDistance_;
				}
			}

			if (tool_ == TransformTool::Scale) {
				// ギズモの軸の長さを基準に、移動量を拡縮の倍率に変換する。
				float factor = 1.0f + movement / dragAxisLength_;

				// 入力状態を取得。
				if (isSnapEnabled_ || ImGui::GetIO().KeyCtrl) {
					factor = 1.0f + std::round((factor-1.0f)/snapScale_)*snapScale_;
				}

				// 軸が潰れたり反転したりしないよう、倍率を正の範囲に収める。
				factor = (std::clamp)(factor,0.01f,100.0f);

				if (isGroupDrag_) {
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
					// 毎フレーム、ドラッグ開始時の値から計算して倍率の累積を防ぐ。
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

	// マウス押下を確認。
	if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
		ResetInteraction();
	} else {
		// ギズモ操作中のUndoなどで、編集中の対象が切り替わることを防ぐ。
		// キーボード入力を捕捉。
		ImGui::SetNextFrameWantCaptureKeyboard(true);
	}

#else
	(void)scene;
	(void)viewInfo;
	(void)mousePosition;

#endif
}

//=============================================================================================//
// Transform更新・復元


void SceneViewPanel::CancelDrag(IEditableWorld& scene) {
	// 操作前のTransformへ戻し、ドラッグ状態を解除する。
	for (const DragObject& target : dragObjects_) {
		const EditableObjectView object = FindObject(scene, target.identifier);

		if (object.transform != nullptr) {
			*object.transform = target.local;
		}
	}

	ResetInteraction();
}

bool SceneViewPanel::ApplyDragTransform(IEditableWorld& scene, const Matrix4x4& delta) {
	// ドラッグの変換行列を選択ルートへ適用する。
	for (const DragObject& target : dragObjects_) {
		if (!scene.SetObjectWorldTransform(target.identifier, MathUtility::Multiply(target.world, delta))) {
			// 一つでも適用できなければ、途中まで反映した対象も操作前の状態へ戻す。
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

//=============================================================================================//
// ID検索・選択・Asset配置


// 選択・Asset配置の窓口。オブジェクトへの借用参照は操作のたびに取得し直す。
EditableObjectView SceneViewPanel::FindObject(IEditableWorld& scene, WorldObjectId objectId) {
	// 指定IDに一致する編集対象を一覧から探す。
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.id == objectId) {
			return object;
		}
	}

	return {kInvalidWorldObjectId, ""};
}

//=============================================================================================//
// 表示領域


// 編集カメラ・選択・配置・ギズモを描く。ウィンドウとタブの開始終了は呼出元が管理する。
void SceneViewPanel::DrawScene(IEditableWorld& scene, EditorSelection& selection) {
#ifdef USE_IMGUI
	EditableObjectView selectedObject = FindObject(scene, selection.GetPrimarySelectedObjectId());

	// 移動・回転・拡縮の切替と、表示・スナップ設定を行う。
	int selectedTool = static_cast<int>(tool_);

	// 次項目の幅を設定。
	ImGui::SetNextItemWidth(100);
	// 選択肢を表示。
	if (ImGui::Combo("##TransformTool", &selectedTool, "Move\0Rotate\0Scale\0")) {
		CancelDrag(scene);

		tool_ = static_cast<TransformTool>(selectedTool);
	}

	// ホバー状態を確認。
	if (ImGui::IsItemHovered()) {
		// 補足説明を表示。
		ImGui::SetTooltip("軸をドラッグして操作 / Ctrl: スナップ / Esc: 操作を戻す");
	}

	// 横並びに配置。
	ImGui::SameLine();
	// 「View / 表示」ボタン押下を受け取る。
	if (ImGui::Button("View / 表示")) {
		// 「SceneViewOptions」ポップアップを開く。
		ImGui::OpenPopup("SceneViewOptions");
	}

	// 「SceneViewOptions」ポップアップを開始。
	if (ImGui::BeginPopup("SceneViewOptions")) {
		// 「Grid」メニュー選択を受け取る。
		ImGui::MenuItem("Grid",nullptr,&shouldShowGrid_);
		// 「Camera / Light Guides」メニュー選択を受け取る。
		ImGui::MenuItem("Camera / Light Guides",nullptr,&shouldShowGuides_);
		// 区切り線を表示。
		ImGui::Separator();
		// 「Snap / 常時スナップ」メニュー選択を受け取る。
		ImGui::MenuItem("Snap / 常時スナップ",nullptr,&isSnapEnabled_);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(100);
		// 「Move」数値をドラッグ編集。
		ImGui::DragFloat("Move",&snapDistance_,0.1f,0.01f,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(100);
		// 「Rotate (deg)」数値をドラッグ編集。
		ImGui::DragFloat("Rotate (deg)",&snapAngle_,1,1,180,"%.0f",ImGuiSliderFlags_AlwaysClamp);
		// 次項目の幅を設定。
		ImGui::SetNextItemWidth(100);
		// 「Scale (ratio)」数値をドラッグ編集。
		ImGui::DragFloat("Scale (ratio)",&snapScale_,0.01f,0.01f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
		// 「Ctrlでも一時的に有効 / Escで操作を戻す」補助テキストを表示。
		ImGui::TextDisabled("Ctrlでも一時的に有効 / Escで操作を戻す");
		// 区切り線を表示。
		ImGui::Separator();

		// 選択中にギズモを操作していない場合だけ、カメラを対象へ移動できる。
		bool canFocusSelectedObject = selectedObject.transform != nullptr;
		if (isDragging_) {
			canFocusSelectedObject = false;
		}
		// 「選択対象を注視」メニュー選択を受け取る。
		if (ImGui::MenuItem("選択対象を注視",nullptr,false,canFocusSelectedObject)) {
			scene.FocusEditorCamera(scene.GetObjectWorldPosition(selectedObject.id));
		}

		// ポップアップを終了。
		ImGui::EndPopup();
	}

	// 横並びに配置。
	ImGui::SameLine();
	// テキストを中央揃え。
	ImGui::AlignTextToFramePadding();

	if (isSnapEnabled_) {
		// 「Snap ON」補助テキストを表示。
		ImGui::TextDisabled("Snap ON");
	} else {
		// 「Scene Camera」補助テキストを表示。
		ImGui::TextDisabled("Scene Camera");
	}

	if (!placementStatus_.empty()) {
		// 折り返しテキストを表示。
		ImGui::TextWrapped("%s", placementStatus_.c_str());
	}

	// 残り表示領域を取得。
	if (!ResizeViewport(scene, ImGui::GetContentRegionAvail())) {
		return;
	}

	// 画面位置を取得。
	const ImVec2 viewportOrigin = ImGui::GetCursorScreenPos();

	// 画像・クリック・ギズモはすべて同じ矩形を使う。
	// 画像を表示。
	ImGui::Image(ImTextureRef(static_cast<ImTextureID>(textureHandle_)),
		{static_cast<float>(width_), static_cast<float>(height_)});

	if (AcceptModelDrop(scene, selection, viewportOrigin)) {
		ResetInteraction();

		return; // 借用した選択データは配列の再配置後に使わない。
	}

	// ホバー状態を確認。
	const bool isImageHovered = ImGui::IsItemHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);

	EditorViewInfo viewInfo{};

	if (!scene.GetEditorViewInfo(viewInfo)) {
		ResetInteraction();

		return;
	}

	// 入力状態を取得。
	const Vector2 mousePosition = {ImGui::GetIO().MousePos.x - viewportOrigin.x, ImGui::GetIO().MousePos.y - viewportOrigin.y};

	if (isDragging_) {
		Update(scene, viewInfo, mousePosition);
	}

	// 描画リストを取得。
	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	// 描画範囲を制限。
	drawList.PushClipRect(viewportOrigin, {viewportOrigin.x + viewInfo.viewportWidth, viewportOrigin.y + viewInfo.viewportHeight}, true);

	DrawOverlay(scene,viewInfo,viewportOrigin);

	DrawColliders(scene, selection, viewInfo, viewportOrigin);

	DrawGuides(scene, selection, viewInfo, viewportOrigin);

	const bool wasGizmoClicked = DrawGizmo(scene, selection, viewInfo, viewportOrigin, mousePosition, isImageHovered);

	// 描画範囲制限を解除。
	drawList.PopClipRect();

	if (!isDragging_ && !wasGizmoClicked) {
		if (isImageHovered) {
			// マウスクリックを確認。
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				PickObject(scene, selection, viewInfo, mousePosition);
			}
		}
	}

	// UI操作中か確認。
	scene.SetEditorCameraInput(isImageHovered && !isDragging_ && !ImGui::IsAnyItemActive());

#else
	(void)scene;
	(void)selection;

#endif
}

#ifdef USE_IMGUI
bool SceneViewPanel::ResizeViewport(IEditableWorld& scene, const ImVec2& availableSize) {
	// 表示領域に合わせてScene描画の寸法とカメラを更新する。
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

	// 画像サイズが変わるとドラッグ開始時とマウス座標の対応がずれるため、操作を取り消す。
	if (isDragging_) {
		if (width_ != nextWidth || height_ != nextHeight) {
			CancelDrag(scene);
		}
	}

	width_ = nextWidth;

	height_ = nextHeight;

	scene.SetEditorViewport(width_, height_);

	scene.SetEditorGameView(false);

	return true;
}

#endif

//=============================================================================================//
// 補助表示


#ifdef USE_IMGUI

// グリッドとメッシュを持たない対象のマーカーを重ねる。
void SceneViewPanel::DrawOverlay(IEditableWorld& scene, const EditorViewInfo& viewInfo, const ImVec2& origin) {
	// SceneView上へ操作状態などの補助情報を重ねる。
	// 描画リストを取得。
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

	if (shouldShowGrid_) {
		DrawGrid(drawList,viewInfo,origin);
	}

	DrawObjectMarkers(scene,drawList,viewInfo,origin);
}

#endif

#ifdef USE_IMGUI
void SceneViewPanel::DrawColliders(IEditableWorld& scene, const EditorSelection& selection, const EditorViewInfo& view, const ImVec2& origin) {
	// 有効なColliderを取り出し、種類に応じた補助線を描く。
	for (WorldObjectId identifier : selection.GetIds()) {
		const WorldObjectData* object = scene.GetObjectDataById(identifier);

		if (object == nullptr) {
			continue;
		}

		if (!object->collider.isPresent) {
			continue;
		}

		Matrix4x4 world{};

		WorldCollider shape{};

		if (!scene.GetObjectWorldMatrix(identifier,world)) {
			continue;
		}

		if (!WorldCollision::BuildWorld(object->collider,world,shape)) {
			continue;
		}

		ImU32 color = IM_COL32(80,240,145,255);

		if (scene.IsColliderTouching(identifier)) {
			color = IM_COL32(255,100,90,255);
		}

		if (!object->collider.isEnabled) {
			color = IM_COL32(150,150,150,180);
		} else if (!scene.IsObjectActive(identifier)) {
			color = IM_COL32(150,150,150,180);
		}

		if (shape.shape == ColliderShape::Box) {
			for (size_t corner = 0; corner < 8; ++corner) {
				for (size_t axis = 0; axis < 3; ++axis) {
					const size_t other = corner ^ (size_t{1} << axis);

					if (corner < other) {
						DrawColliderLine(view,origin,WorldCollision::GetCorner(shape,corner),WorldCollision::GetCorner(shape,other),color);
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

#ifdef USE_IMGUI
void SceneViewPanel::DrawGuides(IEditableWorld& scene, const EditorSelection& selection, const EditorViewInfo& view, const ImVec2& origin) {
	if (!shouldShowGuides_) {
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
					// 描画リストを取得。
					SceneViewDrawing::DrawWorldLine(*ImGui::GetWindowDrawList(),view,origin,LightCirclePoint(center,*object.lightRadius,axisIndex,segment),
						LightCirclePoint(center,*object.lightRadius,axisIndex,segment+1),IM_COL32(255,220,100,200),1.5f);
				}
			}
		}
	}
}

#endif

//=============================================================================================//
// ギズモ描画・操作開始


#ifdef USE_IMGUI
bool SceneViewPanel::BeginDrag(IEditableWorld& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const Vector2& mousePosition, const Vector2& screenOrigin, const Vector3& axisOrigin,
	const Vector3& axisDirection, float axisLength, size_t axisIndex) {
	// 操作軸と開始時の配置を記録し、ギズモのドラッグを開始する。
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

	for (WorldObjectId identifier : selection.GetRoots(scene)) {
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

	const EditableObjectView selected = FindObject(scene,selection.GetPrimarySelectedObjectId());

	isGroupDrag_ = selection.GetIds().size() > 1;
	isDragging_ = true;

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

#ifdef USE_IMGUI

// ギズモの描画・ヒット判定と、ドラッグ開始値の取得を担当する。
bool SceneViewPanel::DrawGizmo(IEditableWorld& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const ImVec2& viewportOrigin, const Vector2& mousePosition, bool isImageHovered) {
	const EditableObjectView object = FindObject(scene,selection.GetPrimarySelectedObjectId());

	if (object.transform == nullptr) {
		return false;
	}

	// 描画リストを取得。
	ImDrawList& drawList = *ImGui::GetWindowDrawList();

	// 操作できない種類・ツールの組合せでも、選択枠は維持する。
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		if (selection.DoesContain(scene.GetEditableObject(objectIndex).id)) {
			AABB bounds{};

			if (scene.GetEditableObjectBounds(objectIndex,bounds)) {
				DrawSelectionBounds(drawList,viewInfo,viewportOrigin,bounds);
			}
		}
	}

	if (tool_ != TransformTool::Move) {
		if (object.type == WorldObjectType::PointLight || object.type == WorldObjectType::Particle) {
			return false;
		}
	}

	if (tool_ == TransformTool::Scale) {
		if (object.type == WorldObjectType::Camera || LightTypes::IsLight(object.type)) {
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
		Vector3 direction;

		Vector2 end;

		bool isVisible;
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

		handles[axisIndex].isVisible = SceneViewMath::Project(axisEnd,viewInfo.viewProjection,
			viewInfo.viewportWidth,viewInfo.viewportHeight,handles[axisIndex].end);

		float distance = 1000000;

		if (tool_ == TransformTool::Rotate) {
			handles[axisIndex].isVisible = true;

			distance = GetRotationRingDistance(viewInfo,axisOrigin,axisLength,axisIndex,mousePosition);
		} else if (handles[axisIndex].isVisible) {
			if (std::hypot(handles[axisIndex].end.x-screenOrigin.x,handles[axisIndex].end.y-screenOrigin.y) < 12) {
				handles[axisIndex].isVisible = false;

				continue;
			}

			distance = SceneViewMath::DistanceToSegment(mousePosition,screenOrigin,handles[axisIndex].end);
		}

		if (isImageHovered && distance < nearest) {
			nearest = distance;

			hoveredAxis = axisIndex;
		}
	}

	for (size_t axisIndex = 0; axisIndex < 3; ++axisIndex) {
		if (!handles[axisIndex].isVisible) {
			continue;
		}

		bool isHighlighted = hoveredAxis == axisIndex;

		if (isDragging_) {
			isHighlighted = dragAxisIndex_ == axisIndex;
		}

		ImU32 color = kAxisColors[axisIndex];

		float thickness = 2;

		if (isHighlighted) {
			color = IM_COL32(255,230,90,255);

			thickness = 4;
		}

		if (tool_ == TransformTool::Rotate) {
			DrawRotationRing(viewInfo,viewportOrigin,axisOrigin,axisLength,axisIndex,color,thickness);

			continue;
		}

		const ImVec2 start = SceneViewDrawing::ToDrawPosition(screenOrigin,viewportOrigin);
		const ImVec2 end = SceneViewDrawing::ToDrawPosition(handles[axisIndex].end,viewportOrigin);

		// 線を描画。
		drawList.AddLine(start,end,color,thickness);

		if (tool_ == TransformTool::Scale) {
			// 塗りつぶし矩形を描画。
			drawList.AddRectFilled({end.x-5,end.y-5},{end.x+5,end.y+5},color);
		} else {
			const float length = std::hypot(end.x-start.x,end.y-start.y);

			const ImVec2 direction = {(end.x-start.x)/length,(end.y-start.y)/length};

			// 塗りつぶし三角形を描画。
			drawList.AddTriangleFilled(end,{end.x-direction.x*13-direction.y*5,end.y-direction.y*13+direction.x*5},
				{end.x-direction.x*13+direction.y*5,end.y-direction.y*13-direction.x*5},color);
		}

		// 文字列を描画。
		drawList.AddText({end.x+7,end.y-7},color,kAxisNames[axisIndex]);
	}

	if (isDragging_ || hoveredAxis >= 3) {
		return false;
	}

	// マウスカーソルを設定。
	ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
	// マウスクリックを確認。
	if (!ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
		return false;
	}

	return BeginDrag(scene,selection,viewInfo,mousePosition,screenOrigin,axisOrigin,
		handles[hoveredAxis].direction,axisLength,hoveredAxis);
}

#endif

//=============================================================================================//
// 選択・配置


#ifdef USE_IMGUI
bool SceneViewPanel::AcceptModelDrop(IEditableWorld& scene, EditorSelection& selection, const ImVec2& viewportOrigin) {
	// ドロップされたAssetを検証し、シーンへの配置要求へ変換する。
	// ドロップ受け入れを開始。
	if (!ImGui::BeginDragDropTarget()) {
		return false;
	}

	// ドロップデータを取得。
	const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kType);

	bool isPrefab = false;

	if (payload == nullptr) {
		// ドロップデータを取得。
		payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kPrefabType);

		isPrefab = payload != nullptr;
	}

	std::string model;

	if (payload != nullptr) {
		ModelAssetPayload::Read(payload->Data, payload->DataSize, model);
	}

	// ドロップ受け入れを終了。
	ImGui::EndDragDropTarget();

	if (model.empty()) {
		return false;
	}

	EditorViewInfo view{};

	if (!scene.GetEditorViewInfo(view)) {
		return false;
	}

	Vector3 origin{};
	Vector3 direction{};

	float rayLength = 0;

	// 入力状態を取得。
	const Vector2 mouse = {ImGui::GetIO().MousePos.x - viewportOrigin.x, ImGui::GetIO().MousePos.y - viewportOrigin.y};

	if (!SceneViewMath::CreateRay(mouse, view.viewProjection, view.viewportWidth, view.viewportHeight, origin, direction, &rayLength)) {
		return false;
	}

	float nearest = rayLength + 1;

	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.hasMeshRenderer == nullptr) {
			continue;
		}

		if (!*object.hasMeshRenderer) {
			continue;
		}

		AABB bounds{};

		float distance = 0;

		if (scene.GetEditableObjectBounds(objectIndex, bounds)) {
			if (SceneViewMath::IntersectBounds(origin, direction, bounds, distance)) {
				if (!scene.IntersectObjectSurface(objectIndex, origin, direction, distance)) {
					continue;
				}

				if (distance < nearest) {
					nearest = distance;
				}
			}
		}
	}

	const Vector3 position = SceneViewMath::GetPlacementPosition(origin, direction, rayLength, nearest);

	WorldObjectId placedId = 0;

	if (isPrefab) {
		placedId = PrefabOperations::Place(scene, model, kInvalidWorldObjectId, position, placementStatus_);
	} else {
		placedId = scene.PlaceModel(model, kInvalidWorldObjectId, position, placementStatus_);
	}

	if (placedId == kInvalidWorldObjectId) {
		return false;
	}

	selection.Select(placedId);

	return true;
}

#endif

#ifdef USE_IMGUI
void SceneViewPanel::PickObject(IEditableWorld& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const Vector2& mousePosition) {
	// 画面からレイを作り、最も手前にある選択対象を探す。
	Vector3 rayOrigin{};
	Vector3 rayDirection{};

	float rayLength = 0;

	if (SceneViewMath::CreateRay(mousePosition, viewInfo.viewProjection, viewInfo.viewportWidth, viewInfo.viewportHeight,
		rayOrigin, rayDirection, &rayLength)) {
		WorldObjectId closestObject = kInvalidWorldObjectId;

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

		// 入力状態を取得。
		if (ImGui::GetIO().KeyCtrl) {
			selection.Toggle(closestObject);
		} else {
		selection.Select(closestObject);
		}
	}
}

#endif
