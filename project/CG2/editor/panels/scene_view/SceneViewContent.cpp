#include "SceneViewPanel.h"
#include "../../EditorSelection.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <algorithm>
#endif

// 編集カメラ・選択・配置・ギズモを描く。ウィンドウとタブの開始終了は呼出元が管理する。
void SceneViewPanel::DrawScene(IEditableWorld& scene, EditorSelection& selection) {
#ifdef USE_IMGUI
	EditableObjectView selectedObject = FindObject(scene, selection.Get());
	int selectedTool = static_cast<int>(tool_);
	ImGui::SetNextItemWidth(100);
	if (ImGui::Combo("##TransformTool", &selectedTool, "Move\0Rotate\0Scale\0")) {
		CancelDrag(scene);
		tool_ = static_cast<TransformTool>(selectedTool);
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("軸をドラッグして操作 / Ctrl: スナップ / Esc: 操作を戻す");
	}
	ImGui::SameLine();
	if (ImGui::Button("View / 表示")) {
		ImGui::OpenPopup("SceneViewOptions");
	}
	if (ImGui::BeginPopup("SceneViewOptions")) {
		ImGui::MenuItem("Grid",nullptr,&showGrid_);
		ImGui::MenuItem("Camera / Light Guides",nullptr,&showGuides_);
		ImGui::Separator();
		ImGui::MenuItem("Snap / 常時スナップ",nullptr,&snapEnabled_);
		ImGui::SetNextItemWidth(100);
		ImGui::DragFloat("Move",&snapDistance_,0.1f,0.01f,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
		ImGui::SetNextItemWidth(100);
		ImGui::DragFloat("Rotate (deg)",&snapAngle_,1,1,180,"%.0f",ImGuiSliderFlags_AlwaysClamp);
		ImGui::SetNextItemWidth(100);
		ImGui::DragFloat("Scale (ratio)",&snapScale_,0.01f,0.01f,1,"%.2f",ImGuiSliderFlags_AlwaysClamp);
		ImGui::TextDisabled("Ctrlでも一時的に有効 / Escで操作を戻す");
		ImGui::Separator();
		if (ImGui::MenuItem("選択対象を注視",nullptr,false,selectedObject.transform != nullptr && !dragging_)) {
			scene.FocusEditorCamera(scene.GetObjectWorldPosition(selectedObject.id));
		}
		ImGui::EndPopup();
	}
	ImGui::SameLine();
	ImGui::AlignTextToFramePadding();
	if (snapEnabled_) {
		ImGui::TextDisabled("Snap ON");
	} else {
		ImGui::TextDisabled("Scene Camera");
	}
	if (!placementStatus_.empty()) {
		ImGui::TextWrapped("%s", placementStatus_.c_str());
	}
	if (!ResizeViewport(scene, ImGui::GetContentRegionAvail())) {
		return;
	}
	const ImVec2 viewportOrigin = ImGui::GetCursorScreenPos();
	// 画像・クリック・ギズモはすべて同じ矩形を使う。
	ImGui::Image(ImTextureRef(static_cast<ImTextureID>(textureHandle_)),
		{static_cast<float>(width_), static_cast<float>(height_)});
	if (AcceptModelDrop(scene, selection, viewportOrigin)) {
		ResetInteraction();
		return; // 借用した選択データは配列の再配置後に使わない。
	}
	const bool imageHovered = ImGui::IsItemHovered() && !ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId);
	EditorViewInfo viewInfo;
	if (!scene.GetEditorViewInfo(viewInfo)) {
		ResetInteraction();
		return;
	}
	const Vector2 mousePosition = {ImGui::GetIO().MousePos.x - viewportOrigin.x, ImGui::GetIO().MousePos.y - viewportOrigin.y};
	if (dragging_) {
		Update(scene, viewInfo, mousePosition);
	}

	ImDrawList& drawList = *ImGui::GetWindowDrawList();
	drawList.PushClipRect(viewportOrigin, {viewportOrigin.x + viewInfo.viewportWidth, viewportOrigin.y + viewInfo.viewportHeight}, true);
	DrawOverlay(scene,viewInfo,viewportOrigin);
	DrawColliders(scene, selection, viewInfo, viewportOrigin);
	DrawGuides(scene, selection, viewInfo, viewportOrigin);
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
#else
	(void)scene;
	(void)selection;
#endif
}

#ifdef USE_IMGUI
bool SceneViewPanel::ResizeViewport(IEditableWorld& scene, const ImVec2& availableSize) {
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
