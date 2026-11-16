#include "Editor.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "SceneFile.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

void Editor::Draw(IEditableScene* scene) {
	if (scene_ != scene) {
		selection_.Clear();

		fileStatus_.clear();

		scene_ = scene;
	}

#ifdef USE_IMGUI
	if (ImGui::Begin("Scene")) {
		ImGui::InputText("File Name", fileName_, sizeof(fileName_));

		// シーンを保存する処理
		if (ImGui::Button("Save Scene")) {
			if (scene != nullptr) {
				if (scene->GetEditableObjectCount() > 0) {
					EditableObjectView object = scene->GetEditableObject(0);

					if (object.transform != nullptr) {
						SceneFile sceneFile;

						std::string error;

						sceneFile.Save(fileName_, "GameScene", object.name, *object.transform, error);

						if (error.empty()) {
							fileStatus_ = "Saved.";
						} else {
							fileStatus_ = error;
						}
					}
				}
			}
		}

		// シーンを読み込む処理
		if (ImGui::Button("Load Scene")) {
			if (scene != nullptr) {
				if (scene->GetEditableObjectCount() > 0) {
					EditableObjectView object = scene->GetEditableObject(0);

					if (object.transform != nullptr) {
						SceneFile sceneFile;

						std::string error;

						sceneFile.Load(fileName_, "GameScene", object.name, *object.transform, error);

						if (error.empty()) {
							fileStatus_ = "Loaded.";
						} else {
							fileStatus_ = error;
						}
					}
				}
			}
		}

		// 保存・読み込みの結果を画面に表示
		ImGui::TextWrapped("%s", fileStatus_.c_str());

	}

	ImGui::End();

#endif

	HierarchyPanel::Draw(scene, selection_);

	EditableObjectView selected{};

	if (scene) {
		for (size_t i = 0; i < scene->GetEditableObjectCount(); ++i) {
			auto object = scene->GetEditableObject(i);

			if (object.id != kInvalidSceneObjectId && object.id == selection_.Get()) {
				selected = object;

				break;
			}
		}
	}

	if (selected.id == kInvalidSceneObjectId) {
		selection_.Clear();
	}

	InspectorPanel::Draw(selected);
}
