#include "FrameProfiler.h"
#include "HierarchyPanel.h"
#include "../../EditorSelection.h"
#include "../../AssetDragPayload.h"
#include "serialization/PrefabFile.h"
#include "objects/WorldPrimitives.h"
#include <cstring>
#include <cstdio>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void HierarchyPanel::Initialize() {
	status_.clear();
	contextTarget_ = kInvalidWorldObjectId;
}

// 種類別の上限を確認し、新規作成サブメニューを描く。
void HierarchyPanel::DrawCreateMenu(IEditableWorld& scene, EditorSelection& selection) {
#ifdef USE_IMGUI
	if (!ImGui::BeginMenu("新規作成")) {
		return;
	}
	size_t lightCount = 0;
	size_t spotCount = 0;
	size_t directionalCount = 0;
	size_t particleCount = 0;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.type == WorldObjectType::PointLight) {
			++lightCount;
		}
		if (object.type == WorldObjectType::SpotLight) {
			++spotCount;
		}
		if (object.type == WorldObjectType::DirectionalLight) {
			++directionalCount;
		}
		if (object.type == WorldObjectType::Particle) {
			++particleCount;
		}
	}
	WorldObjectId createdId = kInvalidWorldObjectId;
	if (ImGui::MenuItem("空オブジェクト / Empty")) {
		createdId = scene.CreateObject(WorldObjectType::Empty);
	}
	if (ImGui::BeginMenu("Light / ライト")) {
		ImGui::BeginDisabled(lightCount >= kMaxPointLights);
		if (ImGui::MenuItem("Point Light / 点光源（最大9個）")) {
			createdId = scene.CreateObject(WorldObjectType::PointLight);
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(spotCount >= kMaxSpotLights);
		if (ImGui::MenuItem("Spot Light / スポット（最大8個）")) {
			createdId = scene.CreateObject(WorldObjectType::SpotLight);
		}
		ImGui::EndDisabled();
		ImGui::BeginDisabled(directionalCount >= kMaxDirectionalLights);
		if (ImGui::MenuItem("Directional Light / 平行光源（最大4個）")) {
			createdId = scene.CreateObject(WorldObjectType::DirectionalLight);
		}
		ImGui::EndDisabled();
		ImGui::EndMenu();
	}
	ImGui::BeginDisabled(particleCount >= 32);
	if (ImGui::MenuItem("Particle（最大32個）")) {
		createdId = scene.CreateObject(WorldObjectType::Particle);
	}
	ImGui::EndDisabled();
	if (ImGui::MenuItem("Camera")) {
		createdId = scene.CreateObject(WorldObjectType::Camera);
	}
	ImGui::Separator();
	if (ImGui::BeginMenu("3D Object / 基本形状")) {
		if (ImGui::MenuItem("Cube / 立方体")) {
			createdId = WorldPrimitives::Create(scene,PrimitiveType::Cube,status_);
		}
		if (ImGui::MenuItem("Sphere / 球")) {
			createdId = WorldPrimitives::Create(scene,PrimitiveType::Sphere,status_);
		}
		if (ImGui::MenuItem("Plane")) {
			createdId = scene.AddObject("plane.gltf");
		}
		ImGui::EndMenu();
	}
	if (ImGui::MenuItem("Axis")) {
		createdId = scene.AddObject("axis.obj");
	}
	if (createdId != kInvalidWorldObjectId) {
		selection.Select(createdId);
		status_.clear();
	}
	ImGui::EndMenu();
#else
	(void)scene;
	(void)selection;
#endif
}

// ツリーの描画後に操作するため、削除・追加で描画中の参照を無効化しない。
void HierarchyPanel::DrawContextMenu(IEditableWorld& scene, EditorSelection& selection) {
#ifdef USE_IMGUI
	if (!ImGui::BeginPopup("HierarchyContext")) {
		return;
	}
	DrawCreateMenu(scene, selection);
	if (contextTarget_ != kInvalidWorldObjectId) {
		DrawPrefabSaveMenu(scene);
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const WorldObjectData* object = scene.GetObjectData(objectIndex);
			if (object == nullptr) {
				continue;
			}
			if (object->id != contextTarget_) {
				continue;
			}
			if (object->prefabSource.empty()) {
				continue;
			}
			if (ImGui::BeginMenu("Prefab")) {
				ImGui::TextDisabled("Source: %s", object->prefabSource.c_str());
				ImGui::TextWrapped("Applyは元Assetを上書きします（ファイル変更はUndo対象外）。他の配置はRevertで更新します。");
				if (ImGui::MenuItem("Apply / 元Assetを上書き")) {
					if (PrefabFile::Apply(scene, contextTarget_, status_)) {
						status_ = "Prefab Assetを更新しました。";
					}
				}
				if (ImGui::MenuItem("Revert / Assetから子孫ごと復元")) {
					if (PrefabFile::Revert(scene, contextTarget_, status_)) {
						selection.Select(contextTarget_);
						status_ = "Prefabを復元しました（ルートTransformは維持）。";
					}
				}
				ImGui::EndMenu();
			}
			break;
		}
		ImGui::Separator();
		if (ImGui::MenuItem("複製", "Ctrl+D")) {
			selection.Duplicate(scene, status_);
		}
		if (ImGui::MenuItem("削除", "Delete")) {
			selection.Remove(scene, status_);
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("子オブジェクトもまとめて削除します。Ctrl+Zで元に戻せます。");
		}
		bool hasParent = false;
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);
			if (object.id == contextTarget_) {
				hasParent = object.parentId != kInvalidWorldObjectId;
				break;
			}
		}
		if (ImGui::MenuItem("親を外す", nullptr, false, hasParent)) {
			scene.SetObjectParent(contextTarget_, kInvalidWorldObjectId, status_);
		}
		ImGui::Separator();
		if (ImGui::MenuItem("選択解除")) {
			selection.Clear();
		}
	}
	ImGui::EndPopup();
#else
	(void)scene;
	(void)selection;
#endif
}

// ドロップでvectorが置き換わるのはツリー描画がすべて終わってから。
void HierarchyPanel::AcceptParentDrop(WorldObjectId parentId, HierarchyRequest& request) {
#ifdef USE_IMGUI
	if (ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PON_SCENE_OBJECT");
		if (payload != nullptr) {
			if (payload->DataSize == sizeof(WorldObjectId)) {
				std::memcpy(&request.childId, payload->Data, sizeof(request.childId));
				request.parentId = parentId;
				request.requested = true;
			}
		}
		const ImGuiPayload* modelPayload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kType);
		if (modelPayload == nullptr) {
			modelPayload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kPrefabType);
			if (modelPayload != nullptr) {
				request.prefab = true;
			}
		}
		if (modelPayload != nullptr) {
			if (ModelAssetPayload::Read(modelPayload->Data, modelPayload->DataSize, request.model)) {
				request.parentId = parentId;
			}
		}
		ImGui::EndDragDropTarget();
	}
#else
	(void)parentId;
	(void)request;
#endif
}

void HierarchyPanel::DrawBranch(IEditableWorld& scene, EditorSelection& selection, const ChildIndices& children,
	WorldObjectId parentId, HierarchyRequest& request) {
#ifdef USE_IMGUI
	const ChildIndices::const_iterator found = children.find(parentId);
	if (found == children.end()) {
		return;
	}
	for (size_t objectIndex : found->second) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		const std::string identifier = std::to_string(object.id);
		ImGui::PushID(identifier.c_str());
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (selection.Contains(object.id)) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}
		if (!children.contains(object.id)) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		if (!scene.IsObjectActive(object.id)) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
		}
		const bool expanded = ImGui::TreeNodeEx(object.name, flags);
		if (!scene.IsObjectActive(object.id)) {
			ImGui::PopStyleColor();
		}
		if (ImGui::IsItemClicked()) {
			if (!ImGui::IsItemToggledOpen()) {
				if (ImGui::GetIO().KeyCtrl) {
					selection.Toggle(object.id);
				} else {
				selection.Select(object.id);
				}
			}
		}
		// Windowsのメニューと同じく、右ボタンを離したときに開く。
		if (ImGui::IsItemHovered()) {
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				request.contextRequested = true;
				request.contextObjectId = object.id;
			}
		}
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("PON_SCENE_OBJECT", &object.id, sizeof(object.id));
			ImGui::TextUnformatted(object.name);
			ImGui::EndDragDropSource();
		}
		AcceptParentDrop(object.id, request);
		if (expanded) {
			if (children.contains(object.id)) {
				DrawBranch(scene, selection, children, object.id, request);
				ImGui::TreePop();
			}
		}
		ImGui::PopID();
	}
#else
	(void)scene;
	(void)selection;
	(void)children;
	(void)parentId;
	(void)request;
#endif
}

void HierarchyPanel::Draw(IEditableWorld* scene, EditorSelection& selection) {
	FrameProfiler::Scope sectionTime(FrameProfiler::Section::Hierarchy);
#ifdef USE_IMGUI
	if (!ImGui::Begin("Hierarchy")) {
		ImGui::End();
		return;
	}
	if (scene == nullptr) {
		ImGui::TextUnformatted("No editable scene");
		ImGui::End();
		return;
	}
	if (!scene->SupportsObjectCollection()) {
		ImGui::TextUnformatted("Object editing is unavailable");
		ImGui::End();
		return;
	}
	ImGui::TextDisabled("Scene  /  %zu objects",scene->GetEditableObjectCount());
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("Ctrl+クリックで複数選択 / Ctrl+Dで一括複製 / Deleteで一括削除");
	}
	ImGui::Separator();
	HierarchyRequest request;
	ChildIndices children;
	for (size_t objectIndex = 0; objectIndex < scene->GetEditableObjectCount(); ++objectIndex) {
		children[scene->GetEditableObject(objectIndex).parentId].push_back(objectIndex);
	}
	DrawBranch(*scene, selection, children, kInvalidWorldObjectId, request);
	// 空白部分は目立つ操作行を置かず、そのまま最上位へのドロップ先にする。
	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}
	ImVec2 backgroundSize = ImGui::GetContentRegionAvail();
	if (backgroundSize.y < 20) {
		backgroundSize.y = 20;
	}
	ImGui::Dummy(backgroundSize);
	AcceptParentDrop(kInvalidWorldObjectId, request);
	if (ImGui::IsWindowHovered()) {
		if (!ImGui::IsAnyItemHovered()) {
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				request.contextRequested = true;
				request.contextObjectId = kInvalidWorldObjectId;
			}
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
				selection.Clear();
			}
		}
	}

	if (request.requested) {
		if (scene->SetObjectParent(request.childId, request.parentId, status_)) {
			selection.Select(request.childId);
		}
	}
	if (!request.model.empty()) {
		Vector3 position{};
		if (request.parentId != kInvalidWorldObjectId) {
			position = scene->GetObjectWorldPosition(request.parentId);
		}
		WorldObjectId placedId = 0;
		if (request.prefab) {
			placedId = scene->PlacePrefab(request.model, request.parentId, position, status_);
		} else {
			placedId = scene->PlaceModel(request.model, request.parentId, position, status_);
		}
		if (placedId != kInvalidWorldObjectId) {
			selection.Select(placedId);
		}
	}
	// PopupのIDは各オブジェクトのPushIDの外側で統一する。
	if (request.contextRequested) {
		contextTarget_ = request.contextObjectId;
		std::snprintf(prefabName_, sizeof(prefabName_), "Prefab_%llu", static_cast<unsigned long long>(contextTarget_));
		if (!selection.Contains(contextTarget_)) {
		selection.Select(contextTarget_);
		}
		ImGui::OpenPopup("HierarchyContext");
	}
	DrawContextMenu(*scene, selection);
	ImGui::End();
#else
	(void)scene;
	(void)selection;
#endif
}

// 子孫をまとめて保存する。入力にディレクトリを許さず、resources/prefabs内へ置く。
void HierarchyPanel::DrawPrefabSaveMenu(IEditableWorld& scene) {
#ifdef USE_IMGUI
	if (!ImGui::BeginMenu("Prefabとして保存")) {
		return;
	}
	ImGui::InputText("File Name", prefabName_, sizeof(prefabName_));
	if (ImGui::MenuItem("保存（同名は上書き）")) {
		const std::string name = prefabName_;
		if (name.empty() || name.find_first_of("\\/:*?\"<>|") != std::string::npos || name == "." || name == "..") {
			status_ = "ファイル名に使えない文字が含まれています。";
		} else {
			const std::filesystem::path path = PrefabFile::ResolveSource("prefabs/" + name + ".ponprefab", status_);
			if (path.empty()) {
				ImGui::EndMenu();
				return;
			}
			if (PrefabFile::Save(scene, contextTarget_, path, status_)) {
				std::vector<WorldObjectData> connected = scene.CaptureObjects();
				for (WorldObjectData& object : connected) {
					if (object.id == contextTarget_) {
						object.prefabSource = "prefabs/" + name + ".ponprefab";
					}
				}
				if (scene.ReplaceObjects(std::move(connected), status_)) {
					status_ = "Prefabを保存し、元Assetへ接続しました。右クリックのPrefabからApply/Revertできます。";
				}
			}
		}
	}
	ImGui::EndMenu();
#else
	(void)scene;
#endif
}
