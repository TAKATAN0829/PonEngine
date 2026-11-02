#include "SceneViewPanel.h"
#include "../../EditorSelection.h"
#include "../../AssetDragPayload.h"
#include "SceneViewMath.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <limits>
#endif

// 選択・Asset配置の窓口。オブジェクトへの借用参照は操作のたびに取得し直す。
EditableObjectView SceneViewPanel::FindObject(IEditableWorld& scene, WorldObjectId objectId) {
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == objectId) {
			return object;
		}
	}
	return {};
}

#ifdef USE_IMGUI
void SceneViewPanel::PickObject(IEditableWorld& scene, EditorSelection& selection, const EditorViewInfo& viewInfo,
	const Vector2& mousePosition) {
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
		if (ImGui::GetIO().KeyCtrl) {
			selection.Toggle(closestObject);
		} else {
		selection.Select(closestObject);
		}
	}
}

bool SceneViewPanel::AcceptModelDrop(IEditableWorld& scene, EditorSelection& selection, const ImVec2& viewportOrigin) {
	if (!ImGui::BeginDragDropTarget()) {
		return false;
	}
	const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kType);
	bool prefab = false;
	if (payload == nullptr) {
		payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kPrefabType);
		prefab = payload != nullptr;
	}
	std::string model;
	if (payload != nullptr) {
		ModelAssetPayload::Read(payload->Data, payload->DataSize, model);
	}
	ImGui::EndDragDropTarget();
	if (model.empty()) {
		return false;
	}
	EditorViewInfo view;
	if (!scene.GetEditorViewInfo(view)) {
		return false;
	}
	Vector3 origin{};
	Vector3 direction{};
	float rayLength = 0;
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
	if (prefab) {
		placedId = scene.PlacePrefab(model, kInvalidWorldObjectId, position, placementStatus_);
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
