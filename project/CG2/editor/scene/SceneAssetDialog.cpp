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
	// 読込用の入力状態を用意し、ファイル選択画面を開く予約をする。
	isSaving_ = false;

	input_[0] = '\0';

	status_.clear();

	isOverwritePending_ = false;

	isOpenRequested_ = true;
}

void SceneAssetDialog::OpenForSave(const std::filesystem::path& suggestion) {
	// 保存先の候補を入力欄へ入れ、保存用の選択画面を予約する。
	isSaving_ = true;

	const std::string text = AssetPathChange::Text(suggestion);

	std::snprintf(input_, sizeof(input_), "%s", text.c_str());

	status_.clear();

	isOverwritePending_ = false;

	isOpenRequested_ = true;
}

void SceneAssetDialog::Draw(const SceneDocument& document) {
	// 読込と保存の用途を切り替え、パス入力とScene一覧を表示する。
#ifdef USE_IMGUI
	const bool shouldFocusInput = isOpenRequested_;

	// 開く要求が来たときだけファイル一覧を走査する。
	if (isOpenRequested_) {
		Scan(document);

		isSelectionReady_ = false;

		wasCancelled_ = false;

		ImGui::OpenPopup("Sceneファイル");

		isOpenRequested_ = false;
	}

	if (!ImGui::BeginPopupModal("Sceneファイル", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}

	const char* action = "開く";

	if (isSaving_) {
		action = "保存";
	}

	ImGui::TextUnformatted("resourcesからの相対パス（例: scenes/Stage01.ponscene）");

	ImGui::SetNextItemWidth(480);

	// 開いた直後はパス入力へフォーカスを移す。
	if (shouldFocusInput) {
		ImGui::SetKeyboardFocusHere();
	}

	const bool wasPathSubmitted = ImGui::InputText("##ScenePath", input_, sizeof(input_), ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);

	if (ImGui::IsItemEdited()) {
		isOverwritePending_ = false;
	}

	if (wasPathSubmitted) {
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

			isOverwritePending_ = false;

			if (!isSaving_) {
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

	// 上書き確認が必要な選択は、明示的な許可を待つ。
	if (isOverwritePending_) {
		ImGui::TextUnformatted("既存のSceneファイルを上書きしますか？");

		if (ImGui::Button("上書きする")) {
			Submit(document, true);
		}

		ImGui::SameLine();

		if (ImGui::Button("戻る")) {
			isOverwritePending_ = false;
		}
	} else {
		if (ImGui::Button(action)) {
			Submit(document, false);
		}
	}

	ImGui::SameLine();

	if (ImGui::Button("キャンセル")) {
		wasCancelled_ = true;

		ImGui::CloseCurrentPopup();
	}

	if (isSelectionReady_) {
		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
#else
	(void)document;
#endif
}

//=============================================================================================//
// Getter / 選択結果

bool SceneAssetDialog::ConsumeSelection(std::filesystem::path& path, bool& isOverwriteConfirmed) {
	// 選択したパスと上書き確認の結果を取り出す。
	if (!isSelectionReady_) {
		return false;
	}

	path = selected_;

	isOverwriteConfirmed = isSelectedOverwriteConfirmed_;

	isSelectionReady_ = false;

	return true;
}

bool SceneAssetDialog::ConsumeCancelled() {
	// ファイル選択の取消要求を取り出し、要求を解除する。
	const bool wasCancelled = wasCancelled_;

	wasCancelled_ = false;

	return wasCancelled;
}

void SceneAssetDialog::SetStatus(const std::string& status) {
	// 直近の操作結果の通知を更新する。
	status_ = status;
}

//=============================================================================================//
// 候補の走査・選択

void SceneAssetDialog::Scan(const SceneDocument& document) {
	// ゴミ箱とリンク先を除外し、選択できるSceneファイルを集める。
	scenes_.clear();

	std::error_code error;

	std::filesystem::recursive_directory_iterator iterator(document.GetRoot(), error);

	const std::filesystem::recursive_directory_iterator end;

	size_t inspected = 0;

	while (!error && iterator != end) {
		++inspected;

		// 走査量を制限し、大きなフォルダでUIが長時間止まるのを防ぐ。
		if (inspected > 10000 || scenes_.size() >= 512) {
			status_ = "一覧の上限です。必要なSceneは相対パスで指定できます。";

			break;
		}

		const DWORD attributes = GetFileAttributesW(iterator->path().c_str());

		bool shouldSkip = AssetPathChange::DoesContain(".editor-trash", iterator->path().filename());

		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				shouldSkip = true;
			}
		}

		if (shouldSkip) {
			// ゴミ箱とリンク先の中へは入らない。
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

	// 表示するSceneの順序を揃える。
	std::sort(scenes_.begin(), scenes_.end());
}

void SceneAssetDialog::Submit(const SceneDocument& document, bool isOverwriteConfirmed) {
	// 入力パスを検証し、必要な上書き確認が済んだ選択結果を返す。
	if (!document.ResolvePath(AssetPathChange::Path(input_), !isSaving_, selected_, status_)) {
		return;
	}

	if (isSaving_) {
		std::error_code error;

		if (std::filesystem::exists(selected_, error)) {
			const bool isCurrentFile = AssetPathChange::DoesContain(document.GetPath(), selected_) && AssetPathChange::DoesContain(selected_, document.GetPath());

			if (!isCurrentFile) {
				if (!isOverwriteConfirmed) {
					isOverwritePending_ = true;

					return;
				}
			}
		}

		if (error) {
			status_ = error.message();

			return;
		}
	}

	isSelectedOverwriteConfirmed_ = isOverwriteConfirmed;

	isSelectionReady_ = true;
}
