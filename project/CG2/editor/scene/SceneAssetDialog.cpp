#include "SceneAssetDialog.h"
#include <Windows.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// 表示要求・描画

void SceneAssetDialog::OpenForLoad() {
	saving_ = false;

	input_[0] = '\0';

	status_.clear();

	overwritePending_ = false;

	openRequested_ = true;
}

void SceneAssetDialog::OpenForSave(const std::filesystem::path& suggestion) {
	saving_ = true;

	const std::string text = AssetPathChange::Text(suggestion);

	std::snprintf(input_, sizeof(input_), "%s", text.c_str());

	status_.clear();

	overwritePending_ = false;

	openRequested_ = true;
}

void SceneAssetDialog::Draw(const SceneDocument& document) {
#ifdef USE_IMGUI
	const bool focusInput = openRequested_;

	if (openRequested_) {
		Scan(document);

		selectionReady_ = false;

		cancelled_ = false;

		ImGui::OpenPopup("Sceneファイル");

		openRequested_ = false;
	}

	if (!ImGui::BeginPopupModal("Sceneファイル", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}

	const char* action = "開く";

	if (saving_) {
		action = "保存";
	}

	ImGui::TextUnformatted("resourcesからの相対パス（例: scenes/Stage01.ponscene）");

	ImGui::SetNextItemWidth(480);

	if (focusInput) {
		ImGui::SetKeyboardFocusHere();
	}

	const bool submitPath = ImGui::InputText("##ScenePath", input_, sizeof(input_), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

	if (ImGui::IsItemEdited()) {
		overwritePending_ = false;
	}

	if (submitPath) {
		Submit(document, false);
	}

	if (ImGui::Button("一覧を更新")) {
		Scan(document);
	}

	ImGui::BeginChild("SceneFiles", {520, 210}, ImGuiChildFlags_Borders);

	for (const std::filesystem::path& scene : scenes_) {
		const std::string label = AssetPathChange::Text(scene);

		if (ImGui::Selectable(label.c_str(), label == input_, ImGuiSelectableFlags_AllowDoubleClick)) {
			std::snprintf(input_, sizeof(input_), "%s", label.c_str());

			overwritePending_ = false;

			if (!saving_) {
				if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
					Submit(document, false);
				}
			}
		}
	}

	ImGui::EndChild();

	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}

	if (overwritePending_) {
		ImGui::TextUnformatted("既存のSceneファイルを上書きしますか？");

		if (ImGui::Button("上書きする")) {
			Submit(document, true);
		}

		ImGui::SameLine();

		if (ImGui::Button("戻る")) {
			overwritePending_ = false;
		}
	} else {
		if (ImGui::Button(action)) {
			Submit(document, false);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("キャンセル")) {
		cancelled_ = true;

		ImGui::CloseCurrentPopup();
	}

	if (selectionReady_) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
#else
	(void)document;
#endif
}

//=============================================================================================//
// Getter / 選択結果

bool SceneAssetDialog::ConsumeSelection(std::filesystem::path& path, bool& overwriteConfirmed) {
	if (!selectionReady_) {
		return false;
	}

	path = selected_;

	overwriteConfirmed = selectedOverwrite_;

	selectionReady_ = false;

	return true;
}

bool SceneAssetDialog::ConsumeCancelled() {
	const bool cancelled = cancelled_;

	cancelled_ = false;

	return cancelled;
}

//=============================================================================================//
// 候補の走査・選択

void SceneAssetDialog::Scan(const SceneDocument& document) {
	scenes_.clear();

	std::error_code error;

	std::filesystem::recursive_directory_iterator iterator(document.GetRoot(), error);

	const std::filesystem::recursive_directory_iterator end;

	size_t inspected = 0;

	while (!error && iterator != end) {
		++inspected;

		if (inspected > 10000 || scenes_.size() >= 512) {
			status_ = "一覧の上限です。必要なSceneは相対パスで指定できます。";

			break;
		}

		const DWORD attributes = GetFileAttributesW(iterator->path().c_str());

		bool skip = AssetPathChange::Contains(".editor-trash", iterator->path().filename());

		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				skip = true;
			}
		}

		if (skip) {
			iterator.disable_recursion_pending();
		} else {
			if (iterator->is_regular_file(error)) {
				std::string extension = AssetPathChange::Text(iterator->path().extension());

				for (char& character : extension) {
					character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
				}

				if (extension == ".ponscene") {
					scenes_.push_back(iterator->path().lexically_relative(document.GetRoot()));
				}
			}
		}

		iterator.increment(error);
	}

	if (error) {
		status_ = error.message();
	}

	std::sort(scenes_.begin(), scenes_.end());
}

void SceneAssetDialog::Submit(const SceneDocument& document, bool overwriteConfirmed) {
	if (!document.ResolvePath(AssetPathChange::Path(input_), !saving_, selected_, status_)) {
		return;
	}

	if (saving_) {
		std::error_code error;

		if (std::filesystem::exists(selected_, error)) {
			const bool currentFile = AssetPathChange::Contains(document.GetPath(), selected_) && AssetPathChange::Contains(selected_, document.GetPath());

			if (!currentFile) {
				if (!overwriteConfirmed) {
					overwritePending_ = true;

					return;
				}
			}
		}

		if (error) {
			status_ = error.message();

			return;
		}
	}

	selectedOverwrite_ = overwriteConfirmed;

	selectionReady_ = true;
}
