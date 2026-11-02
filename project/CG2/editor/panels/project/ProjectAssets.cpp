#include "ProjectPanel.h"
#include "../../EditorSelection.h"
#include "../../AssetDragPayload.h"
#include "objects/WorldObjects.h"
#include <algorithm>
#include <cstring>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "TextureManager.h"
#endif

// Assetタイル・サムネイル・右クリック・ドラッグ＆ドロップを担当する。
ProjectPanel::Preview ProjectPanel::GetPreview(const AssetEntry& entry, IEditableWorld& scene) {
#ifdef USE_IMGUI
	const std::string key = PathText(entry.path);
	if (previews_.contains(key)) {
		return previews_.at(key);
	}
	if (entry.directory) {
		return {};
	}
	const std::string model = PathText(entry.path.lexically_relative(root_));
	if (WorldObjects::IsSupportedModel(model)) {
		if (previewLoadedThisFrame_) {
			return {};
		}
		previewLoadedThisFrame_ = true;
		Preview modelPreview;
		modelPreview.handle = scene.GetModelPreview(model,status_);
		modelPreview.width = 128;
		modelPreview.height = 128;
		previews_[key] = modelPreview;
		return modelPreview;
	}
	if (entry.extension != ".png" && entry.extension != ".jpg" && entry.extension != ".jpeg" && entry.extension != ".bmp") {
		return {};
	}
	if (previewLoadedThisFrame_) {
		return {};
	}
	if (previews_.size() >= 64) {
		return {};
	}
	previewLoadedThisFrame_ = true;
	Preview preview;
	if (TextureManager::GetInstance()->TryLoad(key, status_)) {
		preview.handle = TextureManager::GetInstance()->GetSrvHandleGPU(key).ptr;
		preview.width = static_cast<float>(TextureManager::GetInstance()->GetMetaData(key).width);
		preview.height = static_cast<float>(TextureManager::GetInstance()->GetMetaData(key).height);
	}
	previews_[key] = preview;
	return preview;
#else
	(void)entry;
	(void)scene;
	return {};
#endif
}

void ProjectPanel::DrawAssetActions(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
#ifdef USE_IMGUI
	if (entry.directory) {
		if (ImGui::MenuItem("開く")) {
			OpenDirectory(entry.path);
		}
		return;
	}
	const std::string relative = PathText(entry.path.lexically_relative(root_));
	if (entry.extension == ".ponprefab") {
		if (ImGui::MenuItem("Prefabをシーンに配置")) {
			scene.PlacePrefab(relative, 0, {}, status_);
		}
		return;
	}
	if (WorldObjects::IsSupportedModel(relative)) {
		if (ImGui::MenuItem("シーンに配置")) {
			if (scene.PrepareModel(relative, status_)) {
				scene.AddObject(relative);
			}
		}
		if (ImGui::MenuItem("選択中のMesh Rendererへ適用")) {
			for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
				const EditableObjectView object = scene.GetEditableObject(objectIndex);
				if (object.id == selection.Get()) {
					if (object.type == WorldObjectType::Model || object.type == WorldObjectType::Empty) {
						if (scene.PrepareModel(relative, status_)) {
							*object.model = relative;
							*object.hasMeshRenderer = true;
							if (object.materials != nullptr) {
								object.materials->clear();
							}
							if (object.animation != nullptr) {
								*object.animation = {};
							}
						}
					}
					break;
				}
			}
		}
	} else if (entry.extension == ".ponparticle") {
		if (ImGui::MenuItem("Particle Editorを開く")) {
			visibility.particleEditor = true;
		}
	} else {
		ImGui::TextDisabled("プレビュー・ファイル情報の表示");
	}
#else
	(void)entry; (void)scene; (void)selection; (void)visibility;
#endif
}

void ProjectPanel::DrawAsset(const AssetEntry& entry, IEditableWorld& scene, const EditorSelection& selection, EditorToolVisibility& visibility) {
#ifdef USE_IMGUI
	ImGui::PushID(entry.name.c_str());
	ImGui::BeginGroup();
	const ImVec2 origin = ImGui::GetCursorScreenPos();
	ImGui::Selectable("##Asset", selected_ == entry.path, ImGuiSelectableFlags_AllowDoubleClick, {92, 94});
	if (ImGui::IsItemClicked()) {
		selected_ = entry.path;
	}
	if (!entry.directory) {
		const std::string model = PathText(entry.path.lexically_relative(root_));
		const char* payloadType = nullptr;
		if (WorldObjects::IsSupportedModel(model)) {
			payloadType = ModelAssetPayload::kType;
		} else if (entry.extension == ".ponprefab") {
			payloadType = ModelAssetPayload::kPrefabType;
		} else if (entry.extension == ".png" || entry.extension == ".jpg" || entry.extension == ".jpeg" || entry.extension == ".bmp") {
			payloadType = ModelAssetPayload::kTextureType;
		}
		if (payloadType != nullptr && model.size() < sizeof(ModelAssetPayload::model)) {
			if (ImGui::BeginDragDropSource()) {
				ModelAssetPayload payload;
				std::memcpy(payload.model, model.c_str(), model.size() + 1);
				ImGui::SetDragDropPayload(payloadType, &payload, sizeof(payload));
				ImGui::Text("配置: %s", entry.name.c_str());
				ImGui::EndDragDropSource();
			}
		}
	}
	if (ImGui::IsItemHovered()) {
		ImGui::SetTooltip("%s", entry.name.c_str());
		if (entry.directory) {
			if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				OpenDirectory(entry.path);
			}
		}
	}
	if (ImGui::BeginPopupContextItem("AssetContext")) {
		selected_ = entry.path;
		DrawAssetActions(entry, scene, selection, visibility);
		ImGui::EndPopup();
	}
	if (!ImGui::IsRectVisible(origin, {origin.x + 92, origin.y + 94})) {
		ImGui::EndGroup();
		ImGui::PopID();
		return;
	}
	ImDrawList* drawList = ImGui::GetWindowDrawList();
	const Preview preview = GetPreview(entry, scene);
	if (preview.handle != 0) {
		const float scale = (std::min)(64.0f / preview.width, 64.0f / preview.height);
		const ImVec2 size = {preview.width * scale, preview.height * scale};
		const ImVec2 minimum = {origin.x + 46 - size.x * 0.5f, origin.y + 34 - size.y * 0.5f};
		drawList->AddImage(ImTextureRef(static_cast<ImTextureID>(preview.handle)), minimum, {minimum.x + size.x, minimum.y + size.y});
	} else if (entry.directory) {
		drawList->AddRectFilled({origin.x+16,origin.y+17},{origin.x+40,origin.y+25},IM_COL32(210,173,87,255),3);
		drawList->AddRectFilled({origin.x+14,origin.y+23},{origin.x+78,origin.y+61},IM_COL32(224,187,101,255),4);
	} else {
		drawList->AddRect({origin.x+24,origin.y+10},{origin.x+68,origin.y+63},IM_COL32(150,173,197,255),3.0f,2.0f,ImDrawFlags_None);
		const char* typeName = "FILE";
		if (entry.extension == ".obj" || entry.extension == ".gltf") {
			typeName = "3D";
		}
		if (entry.extension == ".ponscene") {
			typeName = "SCENE";
		}
		if (entry.extension == ".ponparticle") {
			typeName = "FX";
		}
		if (entry.extension == ".ponprefab") {
			typeName = "PREFAB";
		}
		if (entry.extension == ".wav") {
			typeName = "AUDIO";
		}
		drawList->AddText({origin.x+28,origin.y+30},IM_COL32(205,215,225,255),typeName);
	}
	drawList->PushClipRect({origin.x+2,origin.y+70},{origin.x+90,origin.y+94},true);
	drawList->AddText({origin.x+3,origin.y+72},ImGui::GetColorU32(ImGuiCol_Text),entry.name.c_str());
	drawList->PopClipRect();
	ImGui::EndGroup();
	ImGui::PopID();
#else
	(void)entry; (void)scene; (void)selection; (void)visibility;
#endif
}
