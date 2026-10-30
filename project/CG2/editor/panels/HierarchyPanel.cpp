#include "HierarchyPanel.h"
#include "../EditorSelection.h"
#include <cstring>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void HierarchyPanel::Initialize() {
	status_.clear();
	contextTarget_ = kInvalidSceneObjectId;
}

// 種類別の上限を先に表示し、追加に失敗した状態もユーザーへ知らせる。
void HierarchyPanel::DrawCreateMenu(IEditableScene& scene, EditorSelection& selection) {
#ifdef USE_IMGUI
	if (!ImGui::BeginMenu("新規作成")) {
		return;
	}
	size_t lightCount = 0;
	size_t particleCount = 0;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.type == SceneObjectType::PointLight) {
			++lightCount;
		}
		if (object.type == SceneObjectType::Particle) {
			++particleCount;
		}
	}
	SceneObjectId createdId = kInvalidSceneObjectId;
	if (ImGui::MenuItem("空オブジェクト / Empty")) {
		createdId = scene.CreateObject(SceneObjectType::Empty);
	}
	ImGui::BeginDisabled(lightCount >= 8);
	if (ImGui::MenuItem("点光源 / Point Light（最大8個）")) {
		createdId = scene.CreateObject(SceneObjectType::PointLight);
	}
	ImGui::EndDisabled();
	ImGui::BeginDisabled(particleCount >= 32);
	if (ImGui::MenuItem("Particle（最大32個）")) {
		createdId = scene.CreateObject(SceneObjectType::Particle);
	}
	ImGui::EndDisabled();
	ImGui::Separator();
	if (ImGui::MenuItem("Plane")) {
		createdId = scene.AddObject("plane.gltf");
	}
	if (ImGui::MenuItem("Axis")) {
		createdId = scene.AddObject("axis.obj");
	}
	if (createdId != kInvalidSceneObjectId) {
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
void HierarchyPanel::DrawContextMenu(IEditableScene& scene, EditorSelection& selection) {
#ifdef USE_IMGUI
	if (!ImGui::BeginPopup("HierarchyContext")) {
		return;
	}
	DrawCreateMenu(scene, selection);
	if (contextTarget_ != kInvalidSceneObjectId) {
		ImGui::Separator();
		if (ImGui::MenuItem("複製", "Ctrl+D")) {
			const SceneObjectId copiedId = scene.DuplicateObject(contextTarget_);
			if (copiedId != kInvalidSceneObjectId) {
				selection.Select(copiedId);
				status_.clear();
			} else {
				status_ = "複製できません。子を含めたオブジェクト・光源・Particleの上限を確認してください。";
			}
		}
		if (ImGui::MenuItem("削除", "Delete")) {
			if (scene.RemoveObject(contextTarget_)) {
				selection.Clear();
				status_.clear();
			}
		}
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("子オブジェクトもまとめて削除します。Ctrl+Zで元に戻せます。");
		}
		bool hasParent = false;
		for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = scene.GetEditableObject(objectIndex);
			if (object.id == contextTarget_) {
				hasParent = object.parentId != kInvalidSceneObjectId;
				break;
			}
		}
		if (ImGui::MenuItem("親を外す", nullptr, false, hasParent)) {
			scene.SetObjectParent(contextTarget_, kInvalidSceneObjectId, status_);
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
void HierarchyPanel::AcceptParentDrop(SceneObjectId parentId, ParentChangeRequest& request) {
#ifdef USE_IMGUI
	if (ImGui::BeginDragDropTarget()) {
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("PON_SCENE_OBJECT");
		if (payload != nullptr) {
			if (payload->DataSize == sizeof(SceneObjectId)) {
				std::memcpy(&request.childId, payload->Data, sizeof(request.childId));
				request.parentId = parentId;
				request.requested = true;
			}
		}
		ImGui::EndDragDropTarget();
	}
#else
	(void)parentId;
	(void)request;
#endif
}

void HierarchyPanel::DrawBranch(IEditableScene& scene, EditorSelection& selection, const ChildIndices& children,
	SceneObjectId parentId, ParentChangeRequest& request) {
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
		if (selection.Get() == object.id) {
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
				selection.Select(object.id);
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

void HierarchyPanel::Draw(IEditableScene* scene, EditorSelection& selection) {
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
	ImGui::TextDisabled("Objects: %zu", scene->GetEditableObjectCount());
	ImGui::Separator();
	ParentChangeRequest request;
	ChildIndices children;
	for (size_t objectIndex = 0; objectIndex < scene->GetEditableObjectCount(); ++objectIndex) {
		children[scene->GetEditableObject(objectIndex).parentId].push_back(objectIndex);
	}
	DrawBranch(*scene, selection, children, kInvalidSceneObjectId, request);
	// 空白部分は目立つ操作行を置かず、そのまま最上位へのドロップ先にする。
	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}
	ImVec2 backgroundSize = ImGui::GetContentRegionAvail();
	if (backgroundSize.y < 20) {
		backgroundSize.y = 20;
	}
	ImGui::Dummy(backgroundSize);
	AcceptParentDrop(kInvalidSceneObjectId, request);
	if (ImGui::IsWindowHovered()) {
		if (!ImGui::IsAnyItemHovered()) {
			if (ImGui::IsMouseReleased(ImGuiMouseButton_Right)) {
				request.contextRequested = true;
				request.contextObjectId = kInvalidSceneObjectId;
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
	// PopupのIDは各オブジェクトのPushIDの外側で統一する。
	if (request.contextRequested) {
		contextTarget_ = request.contextObjectId;
		selection.Select(contextTarget_);
		ImGui::OpenPopup("HierarchyContext");
	}
	DrawContextMenu(*scene, selection);
	ImGui::End();
#else
	(void)scene;
	(void)selection;
#endif
}
