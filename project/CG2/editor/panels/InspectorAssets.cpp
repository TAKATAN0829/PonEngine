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
	if (!ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
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
		bool changed = ImGui::ColorEdit4("Color", &settings.color.x);
		if (ImGui::DragFloat("Shininess", &settings.shininess, 0.1f, 1, 500, "%.1f", ImGuiSliderFlags_AlwaysClamp)) {
			changed = true;
		}
		if (ImGui::Checkbox("Lighting", &settings.lighting)) {
			changed = true;
		}
		std::string textureLabel = settings.texture;
		if (textureLabel.empty()) {
			textureLabel = imported[slotIndex].texture;
		}
		if (textureLabel.empty()) {
			textureLabel = "White";
		}
		ImGui::TextWrapped("Texture: %s", textureLabel.c_str());
		ImGui::Button("Projectの画像をここへドロップ##Texture", {-1, 0});
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
	if (!ImGui::CollapsingHeader("Animation", ImGuiTreeNodeFlags_DefaultOpen)) {
		return;
	}
	SceneAnimationSettings& settings = *object.animation;
	const char* selected = "クリップを選択";
	if (static_cast<size_t>(settings.clip) < clips.size()) {
		selected = clips[settings.clip].name.c_str();
	}
	if (ImGui::BeginCombo("Clip", selected)) {
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
	ImGui::Checkbox("Play On Start", &settings.autoplay);
	ImGui::Checkbox("Loop", &settings.loop);
	ImGui::DragFloat("Speed", &settings.speed, 0.05f, 0, 10, "%.2f", ImGuiSliderFlags_AlwaysClamp);
	bool previewPlaying = scene.IsAnimationPreviewPlaying(object.id);
	if (ImGui::Checkbox("Preview / Pause", &previewPlaying)) {
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
		if (ImGui::SliderFloat("Time", &position, 0, duration, "%.3f s", ImGuiSliderFlags_AlwaysClamp)) {
			settings.time = position;
			scene.SetAnimationPreview(object.id, false, true);
		}
		ImGui::Text("Duration: %.3f s", duration);
	}
#else
	(void)object;
	(void)scene;
#endif
}
