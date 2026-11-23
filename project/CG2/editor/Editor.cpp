#include "Editor.h"
#include "panels/HierarchyPanel.h"
#include "panels/InspectorPanel.h"
#include "ModelCollection.h"
#include "SceneFile.h"

#include <cstring>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=================================================================================================//
// 初期化処理

void Editor::Initialize() {
	modelCollection_ = nullptr;
	selection_.Clear();
	fileStatus_.clear();
	strcpy_s(fileName_, sizeof(fileName_), "scene.txt");
}

//=================================================================================================//
// 描画処理

void Editor::Draw(ModelCollection* models) {

	//===========================================//
	// 対象の変更確認
	//===========================================//

	if (modelCollection_ != models) {
		selection_.Clear();
		fileStatus_.clear();
		modelCollection_ = models;
	}

#ifdef USE_IMGUI

	//===========================================//
	// 保存・読み込み
	//===========================================//

	if (ImGui::Begin("Scene")) {
		ImGui::InputText("File Name", fileName_, sizeof(fileName_));

		// モデルがないときは保存・読み込みを無効にする
		ImGui::BeginDisabled(models == nullptr || models->GetModels().empty());

		// シーンを保存する処理
		if (ImGui::Button("Save Scene")) {
			if (models != nullptr) {
				SceneFile sceneFile;
				std::string error;
				sceneFile.Save(fileName_, "GameScene", *models, error);

				if (error.empty()) {
					fileStatus_ = "Saved.";
				} else {
					fileStatus_ = error;
				}
			}
		}

		ImGui::EndDisabled();

		// ModelCollectionがないときは読み込みを無効にする
		ImGui::BeginDisabled(models == nullptr);

		// シーンを読み込む処理
		if (ImGui::Button("Load Scene")) {
			if (models != nullptr) {
				SceneFile sceneFile;
				std::string error;
				sceneFile.Load(fileName_, "GameScene", *models, error);

				if (error.empty()) {
					fileStatus_ = "Loaded.";
				} else {
					fileStatus_ = error;
				}
			}
		}

		ImGui::EndDisabled();

		// 保存・読み込みの結果を画面に表示
		ImGui::TextWrapped("%s", fileStatus_.c_str());
	}
	ImGui::End();

#endif

	//===========================================//
	// モデル一覧
	//===========================================//

	HierarchyPanel::Draw(models, selection_);

	//===========================================//
	// 選択モデルの編集
	//===========================================//

	// 選択対象の取得
	SceneModel* selected = nullptr;
	if (models != nullptr) {
		selected = models->FindModel(selection_.Get());
	}

	if (selected == nullptr) {
		selection_.Clear();
	}

	// 選択したモデルを編集
	InspectorPanel::Draw(selected);
}
