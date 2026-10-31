#include "../EditorProperties.h"
#include "InspectorPanel.h"
#include "../AssetDragPayload.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <algorithm>
#endif

// モデル既定値は変更せず、編集されたスロットだけをシーンへ保存する。
void InspectorPanel::DrawMaterials(const EditableObjectView& object, IEditableScene& scene) {
#ifdef USE_IMGUI
	if (object.materials == nullptr || object.model == nullptr) {
		return;
	}
	if (!EditorProperties::Header("Materials","スロットごとの上書き設定です。Texture欄へProjectの画像をドロップできます。右クリックで既定値へ戻せます。")) {
		return;
	}
	const std::vector<ModelMaterialInfo> imported = scene.GetModelMaterials(*object.model);
	for (size_t slotIndex = 0; slotIndex < imported.size(); ++slotIndex) {
		ImGui::PushID(static_cast<int>(slotIndex));
		if (!ImGui::TreeNode(imported[slotIndex].name.c_str())) {
			ImGui::PopID();
			continue;
		}
		SceneMaterialSettings settings;
		settings.slot = static_cast<uint32_t>(slotIndex);
		settings.color = imported[slotIndex].color;
		size_t overrideIndex = object.materials->size();
		for (size_t materialIndex = 0; materialIndex < object.materials->size(); ++materialIndex) {
			if (object.materials->at(materialIndex).slot == slotIndex) {
				overrideIndex = materialIndex;
				settings = object.materials->at(materialIndex);
				break;
			}
		}
		bool changed = false;
		if (EditorProperties::Begin("MaterialFields")) {
			changed = EditorProperties::Color("Color",&settings.color);
			if (EditorProperties::Float("Shininess",&settings.shininess,0.1f,1,500,"%.1f")) {
				changed = true;
			}
			if (EditorProperties::Toggle("Lighting",&settings.lighting)) {
				changed = true;
			}
			EditorProperties::End();
		}
		std::string textureLabel = settings.texture;
		if (textureLabel.empty()) {
			textureLabel = imported[slotIndex].texture;
		}
		if (textureLabel.empty()) {
			textureLabel = "White";
		}
		ImGui::TextDisabled("Texture");
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,{0,0.5f});
		ImGui::Button((textureLabel+"##Texture").c_str(),{-1,0});
		ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s\nProjectの画像をドロップして変更できます。",textureLabel.c_str());
		}
		if (ImGui::BeginDragDropTarget()) {
			const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(ModelAssetPayload::kTextureType);
			std::string texture;
			if (payload != nullptr) {
				if (ModelAssetPayload::Read(payload->Data, payload->DataSize, texture)) {
					if (scene.PrepareTexture(texture, status_)) {
						settings.texture = texture;
						changed = true;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
		bool reset = false;
		if (ImGui::BeginPopupContextItem("MaterialActions")) {
			if (ImGui::MenuItem("モデル既定値へ戻す")) {
				reset = true;
			}
			if (ImGui::MenuItem("テクスチャを白にする")) {
				settings.texture = "white1x1.png";
				if (scene.PrepareTexture(settings.texture, status_)) {
					changed = true;
				}
			}
			ImGui::EndPopup();
		}
		if (reset) {
			if (overrideIndex < object.materials->size()) {
				object.materials->erase(object.materials->begin() + overrideIndex);
			}
		} else if (changed) {
			if (overrideIndex < object.materials->size()) {
				object.materials->at(overrideIndex) = std::move(settings);
			} else {
				object.materials->push_back(std::move(settings));
			}
		}
		ImGui::TreePop();
		ImGui::PopID();
	}
	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}
#else
	(void)object;
	(void)scene;
#endif
}

// 保存する設定と一時プレビューの時計を分け、再生経過がUndoを増やさないようにする。
void InspectorPanel::DrawAnimation(const EditableObjectView& object, IEditableScene& scene) {
#ifdef USE_IMGUI
	if (object.animation == nullptr || object.model == nullptr) {
		return;
	}
	const std::vector<ModelAnimationInfo> clips = scene.GetModelAnimations(*object.model);
	if (clips.empty()) {
		return;
	}
	if (!EditorProperties::Header("Animation","保存する再生設定とプレビューの時計は別管理です。Previewを右クリックして停止・巻き戻しできます。")) {
		return;
	}
	if (!EditorProperties::Begin("AnimationFields")) {
		return;
	}
	SceneAnimationSettings& settings = *object.animation;
	const char* selected = "クリップを選択";
	if (static_cast<size_t>(settings.clip) < clips.size()) {
		selected = clips[settings.clip].name.c_str();
	}
	EditorProperties::Row("Clip");
	if (ImGui::BeginCombo("##Clip", selected)) {
		for (size_t clipIndex = 0; clipIndex < clips.size(); ++clipIndex) {
			ImGui::PushID(static_cast<int>(clipIndex));
			if (ImGui::Selectable(clips[clipIndex].name.c_str(), settings.clip == static_cast<int>(clipIndex))) {
				settings.clip = static_cast<int>(clipIndex);
				settings.time = 0;
				scene.SetAnimationPreview(object.id, false, true);
			}
			ImGui::PopID();
		}
		ImGui::EndCombo();
	}
	EditorProperties::Toggle("Auto Play",&settings.autoplay);
	EditorProperties::Toggle("Loop",&settings.loop);
	EditorProperties::Float("Speed",&settings.speed,0.05f,0,10);
	bool previewPlaying = scene.IsAnimationPreviewPlaying(object.id);
	if (EditorProperties::Toggle("Preview",&previewPlaying)) {
		scene.SetAnimationPreview(object.id, previewPlaying, false);
	}
	if (ImGui::BeginPopupContextItem("AnimationActions")) {
		if (ImGui::MenuItem("停止して開始位置へ戻す")) {
			scene.SetAnimationPreview(object.id, false, true);
		}
		ImGui::EndPopup();
	}
	if (static_cast<size_t>(settings.clip) < clips.size()) {
		const float duration = clips[settings.clip].duration;
		float position = scene.GetAnimationTime(object.id);
		EditorProperties::Row("Time");
		if (ImGui::SliderFloat("##Time", &position, 0, duration, "%.3f s", ImGuiSliderFlags_AlwaysClamp)) {
			settings.time = position;
			scene.SetAnimationPreview(object.id, false, true);
		}
		EditorProperties::Row("Length");
		ImGui::Text("%.3f s",duration);
	}
	EditorProperties::End();
#else
	(void)object;
	(void)scene;
#endif
}
