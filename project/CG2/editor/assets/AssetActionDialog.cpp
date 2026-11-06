#include "AssetActionDialog.h"
#include "../state/EditorSession.h"
#include "AssetDragPayload.h"
#include "../panels/particle_editor/ParticleEditorPanel.h"
#include <cstdio>
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// 初期化処理

void AssetActionDialog::Initialize(const std::filesystem::path& root) {
	// resourcesのルートを保持し、ファイル操作の準備をする。
	root_ = root;

	operations_.Initialize(root, status_);
}

//=============================================================================================//
// 描画処理

void AssetActionDialog::DrawMenu(const std::filesystem::path& source, bool isDirectory, bool areChangesAllowed) {
	// 選択したAssetに対するファイル操作のメニューを作る。
#ifdef USE_IMGUI
	ImGui::Separator();

	bool canModify = areChangesAllowed;

	if (source == root_) {
		canModify = false;
	}

	ImGui::BeginDisabled(!canModify);

	if (ImGui::MenuItem("名前を変更...")) {
		Request(Action::Rename, source);
	}

	if (ImGui::MenuItem("移動...")) {
		Request(Action::Move, source);
	}

	if (ImGui::MenuItem("ゴミ箱へ移動...")) {
		Request(Action::Delete, source);
	}

	ImGui::EndDisabled();

	if (ImGui::MenuItem("使用箇所を確認...", nullptr, false, source != root_)) {
		Request(Action::References, source);
	}

	if (isDirectory) {
		DrawCreateMenu(source, areChangesAllowed);
	}
#else
	(void)source;

	(void)isDirectory;

	(void)areChangesAllowed;
#endif
}

void AssetActionDialog::DrawCreateMenu(const std::filesystem::path& directory, bool areChangesAllowed) {
	// フォルダを作成する操作をメニューへ並べる。
#ifdef USE_IMGUI
	if (ImGui::MenuItem("フォルダを作成...", nullptr, false, areChangesAllowed)) {
		Request(Action::CreateFolder, directory);
	}
#else
	(void)directory;

	(void)areChangesAllowed;
#endif
}

void AssetActionDialog::AcceptMoveDrop(const std::filesystem::path& directory, bool areChangesAllowed) {
	// Projectのドロップ内容から移動先と対象を確認する。
#ifdef USE_IMGUI
	if (!areChangesAllowed) {
		return;
	}

	if (!ImGui::BeginDragDropTarget()) {
		return;
	}

	static const char* payloadTypes[] = {
		ModelAssetPayload::kType, ModelAssetPayload::kTextureType,
		ModelAssetPayload::kPrefabType, ModelAssetPayload::kProjectType
	};

	for (const char* type : payloadTypes) {
		const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type);

		if (payload != nullptr) {
			std::string relative;

			if (ModelAssetPayload::Read(payload->Data, static_cast<size_t>(payload->DataSize), relative)) {
				Request(Action::Move, root_ / AssetPathChange::Path(relative));

				const std::string target = AssetPathChange::Text(directory.lexically_relative(root_));

				std::snprintf(input_, sizeof(input_), "%s", target.c_str());
			}
		}
	}

	ImGui::EndDragDropTarget();
#else
	(void)directory;

	(void)areChangesAllowed;
#endif
}

void AssetActionDialog::Draw(IEditableWorld& scene, EditorSession& history, ParticleEditorPanel& particles, const std::filesystem::path& activeScenePath) {
	// 保留中の要求を開き、使用箇所と実行前の確認を表示する。
	activeScenePath_ = activeScenePath;
#ifdef USE_IMGUI
	if (isOpenRequested_) {
		references_ = CollectReferences(scene, history);

		isInspected_ = true;

		if (action_ != Action::CreateFolder) {
			isInspected_ = operations_.Review(source_, references_, status_);
		}

		ImGui::OpenPopup("Asset操作");

		isOpenRequested_ = false;
	}

	if (!ImGui::BeginPopupModal("Asset操作", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		return;
	}

	const std::string relative = AssetPathChange::Text(source_.lexically_relative(root_));

	ImGui::TextWrapped("resources / %s", relative.c_str());

	if (action_ == Action::Rename || action_ == Action::CreateFolder) {
		ImGui::SetNextItemWidth(380);

		ImGui::InputText("名前", input_, sizeof(input_));
	}

	if (action_ == Action::Move) {
		ImGui::TextUnformatted("移動先のフォルダ（resourcesからの相対パス）");

		ImGui::SetNextItemWidth(380);

		ImGui::InputText("##Destination", input_, sizeof(input_));
	}

	if (action_ != Action::CreateFolder) {
		ImGui::Separator();

		ImGui::Text("外部の使用箇所: %zu", operations_.GetUsages().size());

		ImGui::BeginChild("AssetReferences", {520, 180}, ImGuiChildFlags_Borders);

		for (const AssetReference& usage : operations_.GetUsages()) {
			ImGui::BulletText("%s\n  %s", usage.owner.c_str(), usage.path.c_str());
		}

		ImGui::EndChild();
	}

	if (action_ == Action::Delete) {
		ImGui::TextWrapped("使用中のAssetは削除できません。未使用のAssetはresources/.editor-trashへ移します。");
	} else if (action_ == Action::Rename || action_ == Action::Move) {
		ImGui::TextWrapped("保存データとモデル内の参照、編集中の配置とUndo履歴を更新します。ファイル操作はCtrl+Zの対象外です。");
	}

	if (!status_.empty()) {
		ImGui::TextWrapped("%s", status_.c_str());
	}

	if (action_ != Action::References) {
		bool isAllowed = isInspected_;

		if (history.IsPlaying()) {
			isAllowed = false;
		}

		if (action_ == Action::Delete) {
			if (!operations_.GetUsages().empty()) {
				isAllowed = false;
			}
		}

		ImGui::BeginDisabled(!isAllowed);

		if (ImGui::Button("実行")) {
			if (Execute(scene, history, particles)) {
				action_ = Action::None;

				ImGui::CloseCurrentPopup();
			}
		}

		ImGui::EndDisabled();

		ImGui::SameLine();
	}

	if (ImGui::Button("閉じる")) {
		action_ = Action::None;

		ImGui::CloseCurrentPopup();
	}

	ImGui::EndPopup();
#else
	(void)scene;

	(void)history;

	(void)particles;
#endif
}

//=============================================================================================//
// Getter / 操作結果

const std::string& AssetActionDialog::GetStatus() const {
	// 直近の操作結果の通知を取得する。
	return status_;
}

bool AssetActionDialog::ConsumeRefresh() {
	// 一覧の更新要求を取り出し、要求を解除する。
	const bool isRequested = isRefreshRequested_;

	isRefreshRequested_ = false;

	return isRequested;
}

bool AssetActionDialog::ConsumePathChange(AssetPathChange& change) {
	// パス変更の通知を取り出し、処理済みにする。
	if (!hasPathChanged_) {
		return false;
	}

	change = pathChange_;

	hasPathChanged_ = false;

	return true;
}

//=============================================================================================//
// 操作の準備・確定

void AssetActionDialog::Request(Action action, const std::filesystem::path& source) {
	// 対象と操作種別を保持し、実行前の確認を準備する。
	action_ = action;

	source_ = source;

	std::string initial = AssetPathChange::Text(source.filename());

	if (action == Action::Move) {
		initial = AssetPathChange::Text(source.parent_path().lexically_relative(root_));
	}

	if (action == Action::CreateFolder) {
		initial = "New Folder";
	}

	std::snprintf(input_, sizeof(input_), "%s", initial.c_str());

	status_.clear();

	isOpenRequested_ = true;
}

std::vector<AssetReference> AssetActionDialog::CollectReferences(IEditableWorld& scene, const EditorSession& history) const {
	// 編集中の配置と履歴が参照するAssetを集める。
	std::vector<AssetReference> references;

	WorldAssetReferences::Collect(scene.CaptureObjects(), "Current scene", references);

	history.CollectAssetReferences(references);

	if (!activeScenePath_.empty()) {
		references.push_back({AssetPathChange::Text(activeScenePath_.lexically_relative(root_)), "Editor / open scene document"});
	}

	if (scene.GetParticleSettings() != nullptr) {
		references.push_back({AssetPathChange::Text(AssetPathChange::Path(scene.GetParticleSettings()->texture).lexically_relative("resources")), "Current scene / shared Particle preset"});
	}

	return references;
}

bool AssetActionDialog::Execute(IEditableWorld& scene, EditorSession& history, ParticleEditorPanel& particles) {
	// 確認画面を開いた後の変更も含め、実行直前に使用箇所を調べ直す。
	references_ = CollectReferences(scene, history);

	if (action_ == Action::Delete) {
		std::filesystem::path trash;

		if (!operations_.MoveToTrash(source_, references_, trash, status_)) {
			return false;
		}

		status_ = "ゴミ箱へ移動しました。復元用パス: " + AssetPathChange::Text(trash);
	} else {
		const std::filesystem::path entered = AssetPathChange::Path(input_);

		if (action_ == Action::Rename || action_ == Action::CreateFolder) {
			if (entered.empty() || entered.has_parent_path()) {
				status_ = "フォルダを含めず、名前だけを入力してください。";

				return false;
			}
		}

		if (action_ == Action::CreateFolder) {
			if (!operations_.CreateFolder(source_ / entered, status_)) {
				return false;
			}
		} else {
			// 改名は同じ親の中で行い、移動では選んだフォルダへ元の名前で置く。
			std::filesystem::path destination = source_.parent_path() / entered;

			if (action_ == Action::Move) {
				destination = root_ / entered / source_.filename();
			}

			if (!operations_.Move(source_, destination, scene, references_, status_)) {
				return false;
			}

			// ファイル操作の成功後に、履歴とParticleプリセットの参照も移動先へ合わせる。
			pathChange_ = {root_, source_.lexically_relative(root_), destination.lexically_normal().lexically_relative(root_)};

			history.RemapAssetPaths(pathChange_);

			particles.RemapAssetPaths(pathChange_);

			hasPathChanged_ = true;
		}
	}

	if (action_ == Action::CreateFolder) {
		status_ = "フォルダを作成しました。";
	} else if (action_ != Action::Delete) {
		status_ = "Asset操作が完了しました。参照とUndo履歴を更新しました。";
	}

	isRefreshRequested_ = true;

	return true;
}
