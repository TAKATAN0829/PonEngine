#include "EditorScene.h"
#include "objects/WorldAssetReferences.h"
#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include <Windows.h>
#include <algorithm>
#include <cwctype>
#include <limits>
#include <unordered_map>

namespace {
// 選択ルートとその子孫を一度だけ列挙する。一括操作中は所有スナップショットを使う。
std::unordered_set<WorldObjectId> CollectSubtrees(const std::vector<WorldObjectData>& objects,
												  const std::vector<WorldObjectId>& roots) {
	// 親IDから子を引ける一覧を作る。
	std::unordered_map<WorldObjectId, std::vector<WorldObjectId>> children;

	for (const WorldObjectData& object : objects) {
		children[object.parentId].push_back(object.id);
	}

	// 処理済みのIDを記録し、重複を避ける。
	std::unordered_set<WorldObjectId> included;

	// 選択ルートから子孫へ探索する。
	std::vector<WorldObjectId> pending = roots;

	while (!pending.empty()) {
		// 未処理のIDを一件取り出す。
		const WorldObjectId identifier = pending.back();

		pending.pop_back();

		// 同じオブジェクトは一度だけ処理する。
		if (!included.insert(identifier).second) {
			continue;
		}

		// 子があれば次の探索対象へ追加する。
		const std::unordered_map<WorldObjectId, std::vector<WorldObjectId>>::const_iterator found =
			children.find(identifier);

		if (found != children.end()) {
			pending.insert(pending.end(), found->second.begin(), found->second.end());
		}
	}

	return included;
}
} // namespace

// Scene
void EditorScene::Initialize(IEditableWorld* world, const std::string& initialPath) {
	world_ = world;
	playOrigin_ = nullptr;
	ClearSelection();
	fileStatus_.clear();
	InitializeSceneDocument("resources", AssetPathChange::Path(initialPath), fileStatus_);
	if (world_ != nullptr) {
		InitializeHistory(*world_);
	} else {
		history_.clear();
		historyIndex_ = 0;
		historySavedState_.clear();
		historyPlaySnapshot_.clear();
		isPlaying_ = false;
		isPaused_ = false;
		isStepRequested_ = false;
	}
}

bool EditorScene::IsPlaying() const {
	return isPlaying_;
}

// 一時停止とステップ要求を考慮し、このフレームのゲーム更新可否を返す。
bool EditorScene::ShouldSimulate() {
	// 今回の更新で使うステップ要求を取り出す。
	const bool isStepRequested = isStepRequested_;

	// 同じ要求で次のフレームも更新しない。
	isStepRequested_ = false;

	// 再生中の通常更新、または一時停止中の一回分だけ進める。
	return isPlaying_ && (!isPaused_ || isStepRequested);
}

// 再生中の場面が変わっても、編集前のスナップショットは元の場面にだけ復元する。
IEditableWorld& EditorScene::GetEditWorld(IEditableWorld& scene) const {
	// 再生開始時の編集対象を取得する。
	if (playOrigin_ != nullptr) {
		return *playOrigin_;
	}

	return scene;
}

void EditorScene::InitializeSceneDocument(const std::filesystem::path& resources,
										  const std::filesystem::path& initialPath, std::string& error) {
	// resourcesの実体パスを決め、起動時に読込済みの保存先を引き継ぐ。
	sceneDocumentPath_.clear();

	std::error_code fileError;

	// パス比較の基準となるresourcesの実体を確認する。
	sceneDocumentRoot_ = std::filesystem::canonical(resources, fileError);

	if (fileError) {
		error = "Cannot resolve resources: " + fileError.message();

		return;
	}

	if (!initialPath.empty()) {
		SceneDocumentResolvePath(initialPath, true, sceneDocumentPath_, error);
	} else {
		error.clear();
	}
}

const std::filesystem::path& EditorScene::GetSceneDocumentRoot() const {
	// 操作対象となるresourcesのルートを取得する。
	return sceneDocumentRoot_;
}

const std::filesystem::path& EditorScene::GetSceneDocumentPath() const {
	// 編集中のファイルのパスを取得する。
	return sceneDocumentPath_;
}

bool EditorScene::HasSceneDocumentPath() const {
	// 編集中のシーンに保存先が設定されているか確認する。
	return !sceneDocumentPath_.empty();
}

std::string EditorScene::GetSceneDocumentName() const {
	// 未保存ならUntitled、保存先があれば拡張子を除いた名前を返す。
	if (sceneDocumentPath_.empty()) {
		return "Untitled";
	}

	return AssetPathChange::Text(sceneDocumentPath_.stem());
}

bool EditorScene::SceneDocumentNew(IWorldObjects& objects, IEditorSceneLifecycle& lifecycle, std::string& error) {
	// 空の配置への置換に成功してから、以前の保存先を外す。
	if (!objects.ReplaceObjects({}, error)) {
		return false;
	}

	sceneDocumentPath_.clear();

	lifecycle.OnSceneContentsChanged();

	return true;
}

bool EditorScene::OpenSceneDocument(IWorldObjects& objects, IEditorSceneLifecycle& lifecycle,
									const std::filesystem::path& path, std::string& error) {
	// パスと保存内容を検証し、成功時だけ編集中のファイルを切り替える。
	// 入力されたパスと、検証後に使うパスを分けて持つ。
	std::filesystem::path resolved;

	// 第2引数の true で、既存の .ponscene ファイルを必須にする。
	if (!SceneDocumentResolvePath(path, true, resolved, error)) {
		// パスを確定できなければ、ファイルを開く前に中断する。
		return false;
	}

	// 検証済みのパスを、ファイルを開いて内容を解析する SceneFile に渡す。
	// 最後の true は、旧形式のライト設定を現在の形式に移行する指定。
	if (!SceneFile::Load(objects, resolved, error, true)) {
		// SceneFileが書いた失敗理由をそのまま呼び出し元へ返す。
		return false;
	}

	// 開けたファイルを、以後の上書き保存の対象にする。
	sceneDocumentPath_ = resolved;

	// 前の配置の速度やプレビューを初期化する。
	lifecycle.OnSceneContentsChanged();

	return true;
}

bool EditorScene::SaveSceneDocument(IWorldObjects& objects, std::string& error) {
	// 保存先が決まっていることを確認し、現在のファイルへ保存する。
	if (!HasSceneDocumentPath()) {
		error = "Choose a scene filename before saving.";

		return false;
	}

	return SaveSceneDocumentAs(objects, sceneDocumentPath_, true, error);
}

bool EditorScene::SaveSceneDocumentAs(IWorldObjects& objects, const std::filesystem::path& path,
									  bool isOverwriteConfirmed, std::string& error) {
	// 保存先と上書き可否を確認し、書込成功後に現在のパスを更新する。
	std::filesystem::path resolved;

	// resources内のSceneファイルか検証する。
	if (!SceneDocumentResolvePath(path, false, resolved, error)) {
		return false;
	}

	std::error_code fileError;

	// 別の既存ファイルを上書きする場合は確認を要求する。
	if (std::filesystem::exists(resolved, fileError)) {
		// 両方向に包含するか調べ、同じパスと親子のパスを区別する。
		const bool isCurrentFile = AssetPathChange::DoesContain(sceneDocumentPath_, resolved) &&
								   AssetPathChange::DoesContain(resolved, sceneDocumentPath_);

		if (!isCurrentFile) {
			if (!isOverwriteConfirmed) {
				error = "This scene already exists. Confirm overwrite before saving.";

				return false;
			}
		}
	}

	if (fileError) {
		error = fileError.message();

		return false;
	}

	std::filesystem::path temporary = resolved;

	temporary += L".tmp";

	// 復旧に使える既存の一時ファイルを保護する。
	if (GetFileAttributesW(temporary.c_str()) != INVALID_FILE_ATTRIBUTES) {
		error = "A temporary scene file already exists. Recover or move it before saving.";

		return false;
	}

	if (!SceneFile::Save(objects, resolved, error)) {
		return false;
	}

	// 保存成功後にだけ、今後の保存先を切り替える。
	sceneDocumentPath_ = resolved;

	return true;
}

bool EditorScene::SceneDocumentResolvePath(const std::filesystem::path& requested, bool mustExist,
										   std::filesystem::path& resolved, std::string& error) const try {
	// 検証に失敗したときに、前回のパスが出力先に残らないようにする。
	resolved.clear();

	// 今回の失敗理由を返せるように、前回のメッセージを消す。
	error.clear();

	// resources の基準位置と、利用者が指定したパスの両方が必要。
	if (sceneDocumentRoot_.empty() || requested.empty()) {
		error = "Choose a .ponscene file inside resources.";

		return false;
	}

	// 元の入力を保持したまま、検証用のパスを組み立てる。
	std::filesystem::path candidate = requested;

	if (!candidate.is_absolute()) {
		// APIはresources相対を基本とする。起動時の既存形式resources/...も受け付ける。
		if (*candidate.begin() == "resources") {
			// 先頭の resources を取り除き、基準パスとの二重指定を防ぐ。
			candidate = sceneDocumentRoot_ / candidate.lexically_relative("resources");
		} else {
			// resources からの相対パスを、ファイル操作に使う絶対パスへ変換する。
			candidate = sceneDocumentRoot_ / candidate;
		}
	}

	// 保存時だけ拡張子の省略を許可する。読込時は入力されたファイルを探す。
	if (!mustExist) {
		if (candidate.extension().empty()) {
			candidate += L".ponscene";
		}
	}

	// . や .. を整理したパスで、resourcesの内側に収まるかを調べる。
	candidate = candidate.lexically_normal();

	// resources の外を指すパスと、resources 自体を指すパスを除く。
	if (!AssetPathChange::DoesContain(sceneDocumentRoot_, candidate) || candidate == sceneDocumentRoot_) {
		error = "Scene files must stay inside resources.";

		return false;
	}

	// 拡張子だけを取り出し、実際のファイル名はそのまま保持する。
	std::string extension = AssetPathChange::Text(candidate.extension());

	// .PONSCENE なども同じ形式として扱うため、小文字で比較する。
	for (char& character : extension) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}

	if (extension != ".ponscene") {
		error = "Choose a .ponscene file.";

		return false;
	}

	// 基準パスを取り除き、利用者が指定したフォルダ名とファイル名を検査する。
	const std::filesystem::path relative = candidate.lexically_relative(sceneDocumentRoot_);

	// パスの上限は UTF-8 に変換した後のバイト数で判定する。
	if (AssetPathChange::Text(relative).size() > 240) {
		error = "The scene path exceeds 240 UTF-8 bytes.";

		return false;
	}

	std::filesystem::path component = sceneDocumentRoot_;

	// 最後のファイル名だけでなく、そこへ至るフォルダ名も順番に確認する。
	for (const std::filesystem::path& part : relative) {
		// Windows のパス表現で、区切りごとの名前を取り出す。
		const std::wstring name = part.native();

		// 空の名前、親フォルダの指定、末尾のピリオドや空白を拒否する。
		if (name.empty() || name == L".." || name.back() == L'.' || name.back() == L' ') {
			error = "Invalid scene filename.";

			return false;
		}

		// Editor のゴミ箱と、Windows のファイル名に使えない記号を除く。
		if (AssetPathChange::DoesContain(".editor-trash", part) ||
			name.find_first_of(L"<>:\"|?*") != std::wstring::npos) {
			error = "Reserved or invalid scene path.";

			return false;
		}

		// タブや改行など、目で判別しにくい制御文字も除く。
		for (wchar_t character : name) {
			if (character < 32) {
				error = "Scene filenames cannot contain control characters.";

				return false;
			}
		}

		// NUL.ponscene のように拡張子を付けた名前も、予約名の検査対象にする。
		std::wstring stem = part.stem().native();

		// 大文字と小文字の違いを無視して予約名と比較する。
		for (wchar_t& character : stem) {
			character = std::towupper(character);
		}

		if (stem == L"CON" || stem == L"PRN" || stem == L"AUX" || stem == L"NUL") {
			error = "Windows reserves this filename.";

			return false;
		}

		// COM1〜COM9 と LPT1〜LPT9 も Windows が予約している。
		if (stem.size() == 4) {
			if (stem.starts_with(L"COM") || stem.starts_with(L"LPT")) {
				if (stem.back() >= L'1' && stem.back() <= L'9') {
					error = "Windows reserves this filename.";

					return false;
				}
			}
		}

		// パスを一段ずつ伸ばし、途中のフォルダにも属性検査を行う。
		component /= part;

		// 見かけ上はresources内でも、途中のリンクで別の場所へ飛ぶことを防ぐ。
		const DWORD attributes = GetFileAttributesW(component.c_str());

		// 属性を取得できたときだけ、リンクかどうかをビットで調べる。
		if (attributes != INVALID_FILE_ATTRIBUTES) {
			if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
				error = "Linked scene files and folders cannot be opened or overwritten.";

				return false;
			}
		}
	}

	// ファイルの種類を調べるときのエラーを受け取る。
	std::error_code fileError;

	// 拡張子が .ponscene でも、実体がフォルダなら開けない。
	if (std::filesystem::is_directory(candidate, fileError)) {
		error = "Choose a file, not a folder.";

		return false;
	}

	// 読み込み時は、パスの先に通常のファイルが存在することまで確認する。
	if (mustExist) {
		if (!std::filesystem::is_regular_file(candidate, fileError)) {
			error = "Scene file does not exist.";

			return false;
		}
	}

	// すべての検証を通ったパスを、読み込み側へ返す。
	resolved = candidate;

	return true;
} catch (const std::filesystem::filesystem_error& failure) {
	// パス操作が例外を出した場合も、画面に表示できるメッセージを返す。
	error = failure.what();

	return false;
}

void EditorScene::RemapSceneDocumentAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	if (HasSceneDocumentPath()) {
		sceneDocumentPath_ = AssetPathChange::Path(change.RemapResource(AssetPathChange::Text(sceneDocumentPath_)));
	}
}

WorldObjectId EditorScene::GetPrimarySelectedObjectId() const {
	// 未選択なら無効IDを返す。
	if (selectedObjectIds_.empty()) {
		return kInvalidWorldObjectId;
	}

	// 最後に選んだオブジェクトを主対象にする。
	return selectedObjectIds_.back();
}

const std::vector<WorldObjectId>& EditorScene::GetSelectedObjectIds() const {
	// 選択順を保ったID一覧を参照で返す。
	return selectedObjectIds_;
}

bool EditorScene::IsObjectSelected(WorldObjectId identifier) const {
	// 検索用の集合で選択状態を確認する。
	return selectedObjectIdSet_.contains(identifier);
}

// 親も選択されている子を除き、二重移動・二重複製を防ぐ。
std::vector<WorldObjectId> EditorScene::GetSelectionRoots(const IWorldObjects& objects) const {
	// 親も選択されている子は結果へ含めない。
	std::vector<WorldObjectId> roots;

	for (WorldObjectId identifier : selectedObjectIds_) {
		// すでに削除された選択は飛ばす。
		const WorldObjectData* object = objects.GetObjectDataById(identifier);

		if (object == nullptr) {
			continue;
		}

		// 選択済みの祖先があるか、親をたどる。
		WorldObjectId ancestor = object->parentId;

		while (ancestor != 0) {
			if (IsObjectSelected(ancestor)) {
				break;
			}

			const WorldObjectData* parent = objects.GetObjectDataById(ancestor);

			if (parent == nullptr) {
				break;
			}

			ancestor = parent->parentId;
		}

		// 選択済みの祖先がなければ操作のルートにする。
		if (ancestor == 0) {
			roots.push_back(identifier);
		}
	}

	return roots;
}

void EditorScene::SelectObject(WorldObjectId identifier) {
	// 前の選択を外し、単体選択へ切り替える。
	ClearSelection();

	// 無効IDは選択一覧に入れない。
	if (identifier != 0) {
		// 選択順と検索用の集合を揃えて更新する。
		selectedObjectIds_.push_back(identifier);

		selectedObjectIdSet_.insert(identifier);
	}
}

void EditorScene::ToggleObjectSelection(WorldObjectId identifier) {
	// 無効IDの切替要求は無視する。
	if (identifier == 0) {
		return;
	}

	// 一覧に含まれている位置を調べる。
	const std::vector<WorldObjectId>::iterator found =
		std::find(selectedObjectIds_.begin(), selectedObjectIds_.end(), identifier);

	// 未選択なら追加し、選択済みなら解除する。
	if (found == selectedObjectIds_.end()) {
		selectedObjectIds_.push_back(identifier);

		selectedObjectIdSet_.insert(identifier);
	} else {
		selectedObjectIds_.erase(found);

		selectedObjectIdSet_.erase(identifier);
	}
}

void EditorScene::ClearSelection() {
	// 選択順と検索用の集合を両方空にする。
	selectedObjectIds_.clear();

	selectedObjectIdSet_.clear();
}

// Undo・削除後は、消えたIDだけを選択一覧から除く。
void EditorScene::PruneSelection(const IWorldObjects& objects) {
	// 削除で添字がずれないよう、末尾から確認する。
	for (size_t selectionIndex = selectedObjectIds_.size(); selectionIndex > 0; --selectionIndex) {
		// 実体がないIDは両方の一覧から取り除く。
		if (objects.GetObjectDataById(selectedObjectIds_[selectionIndex - 1]) == nullptr) {
			selectedObjectIdSet_.erase(selectedObjectIds_[selectionIndex - 1]);

			selectedObjectIds_.erase(selectedObjectIds_.begin() + selectionIndex - 1);
		}
	}
}

// 選択数分のシーン全体コピーを避け、採番・親の付け替え・検証を一回で行う。
void EditorScene::DuplicateSelection(IWorldObjects& objects, std::string& error) {
	// 今回の複製処理の結果だけを返す。
	error.clear();

	// 二重複製を避けるため、選択のルートを求める。
	const std::vector<WorldObjectId> roots = GetSelectionRoots(objects);

	// 対象がなければ配置を変更しない。
	if (roots.empty()) {
		return;
	}

	// 元の配置を保持したまま、複製対象の子孫を集める。
	const std::vector<WorldObjectData> original = objects.CaptureObjects();

	const std::unordered_set<WorldObjectId> included = CollectSubtrees(original, roots);

	// 複製後のオブジェクト数を先に検証する。
	if (original.size() + included.size() > WorldObjects::kMaxObjects) {
		error = "Cannot duplicate selection: object limit reached.";

		return;
	}

	// 未使用のIDから、複製分の採番を始める。
	WorldObjectId nextIdentifier = objects.GetNextObjectId();

	// IDの上限を超える複製を防ぐ。
	if (included.size() > (std::numeric_limits<WorldObjectId>::max)() - nextIdentifier) {
		error = "Cannot duplicate selection: ID limit reached.";

		return;
	}

	// 元IDと複製後のIDの対応を作る。
	std::unordered_map<WorldObjectId, WorldObjectId> remapped;

	for (const WorldObjectData& object : original) {
		if (included.contains(object.id)) {
			remapped[object.id] = nextIdentifier++;
		}
	}

	// 元の配置へ複製分を追加する準備をする。
	std::vector<WorldObjectData> candidate = original;

	candidate.reserve(original.size() + included.size());

	for (const WorldObjectData& object : original) {
		// 対象の階層だけを複製する。
		if (!included.contains(object.id)) {
			continue;
		}

		WorldObjectData copy = object;

		// 複製したオブジェクトに新しいIDを設定する。
		copy.id = remapped.at(object.id);

		// 親も複製対象なら、新しい親IDへ接続する。
		if (remapped.contains(copy.parentId)) {
			copy.parentId = remapped.at(copy.parentId);
		}

		// 複製でメインカメラが重複しないようにする。
		copy.isMainCamera = false;

		// 識別用の接尾辞を付ける前に、長い名前を短くする。
		if (copy.name.size() > 200) {
			copy.name = "Object";
		}

		copy.name += " Copy #" + std::to_string(copy.id);

		candidate.push_back(std::move(copy));
	}

	// 全件の検証に成功した場合だけ配置を置き換える。
	if (!objects.ReplaceObjects(std::move(candidate), error)) {
		return;
	}

	// 複製したルートを新しい選択対象にする。
	ClearSelection();

	for (WorldObjectId identifier : roots) {
		selectedObjectIds_.push_back(remapped.at(identifier));

		selectedObjectIdSet_.insert(remapped.at(identifier));
	}
}

void EditorScene::RemoveSelection(IWorldObjects& objects, std::string& error) {
	// 今回の削除処理の結果だけを返す。
	error.clear();

	// 親子の重複を除いた削除ルートを求める。
	const std::vector<WorldObjectId> roots = GetSelectionRoots(objects);

	// 対象がなければ配置を変更しない。
	if (roots.empty()) {
		return;
	}

	// 元の配置から削除対象の子孫を集める。
	std::vector<WorldObjectData> original = objects.CaptureObjects();

	const std::unordered_set<WorldObjectId> included = CollectSubtrees(original, roots);

	// 削除対象以外を新しい配置へ残す。
	std::vector<WorldObjectData> remaining;

	remaining.reserve(original.size());

	for (WorldObjectData& object : original) {
		if (!included.contains(object.id)) {
			remaining.push_back(std::move(object));
		}
	}

	// 検証に失敗した場合は元の配置を維持する。
	if (!objects.ReplaceObjects(std::move(remaining), error)) {
		return;
	}

	// 削除済みのIDを選択状態に残さない。
	ClearSelection();
}

// 読込後のシーンを基準に、履歴・保存状態・再生状態を初期化する。
void EditorScene::InitializeHistory(IWorldObjects& scene) {
	// 以前の履歴を破棄する。
	history_.clear();

	// 読込直後の配置を履歴の先頭にする。
	history_.push_back(scene.CaptureObjects());

	// 現在位置を最初の履歴へ戻す。
	historyIndex_ = 0;

	// 読込直後は保存済みとして扱う。
	historySavedState_ = history_.front();

	// 前の再生で退避した配置を消す。
	historyPlaySnapshot_.clear();

	// 編集モードから開始する。
	isPlaying_ = false;

	// 一時停止とステップ要求を解除する。
	isPaused_ = false;

	isStepRequested_ = false;
}

// ドラッグや文字入力の途中は記録せず、完了した操作を履歴一件にまとめる。
void EditorScene::UpdateHistory(IWorldObjects& scene, bool isInteractionActive) {
	// 再生中の変更は編集履歴へ入れない。
	if (isPlaying_) {
		return;
	}

	// ドラッグや入力が終わるまで記録を待つ。
	if (isInteractionActive) {
		return;
	}

	// 一覧を扱えないシーンでは履歴を作らない。
	if (!scene.HasObjectCollectionSupport()) {
		return;
	}

	// 基準がなければ現在の配置から作る。
	if (history_.empty()) {
		InitializeHistory(scene);

		return;
	}

	// 設定が同じなら履歴を増やさない。
	if (DoesHistoryMatchSnapshot(scene, history_[historyIndex_])) {
		return;
	}

	// Undo後の新しい編集では、古いRedo側の分岐を残さない。
	history_.resize(historyIndex_ + 1);

	// 確定した編集結果を一件追加する。
	history_.push_back(scene.CaptureObjects());

	if (history_.size() > EditorScene::kHistoryMaxHistoryStates) {
		// 古い状態から削除し、大きなシーンでも履歴が無制限に増えないようにする。
		history_.erase(history_.begin());
	}

	// 最新の編集結果を現在位置にする。
	historyIndex_ = history_.size() - 1;
}

// 再生中の一時変更は編集履歴の対象外とし、巻き戻しを禁止する。
bool EditorScene::CanHistoryUndo() const {
	return !isPlaying_ && !history_.empty() && historyIndex_ > 0;
}

// 現在位置より先に履歴が残っている場合だけ、やり直しを許可する。
bool EditorScene::CanHistoryRedo() const {
	return !isPlaying_ && historyIndex_ + 1 < history_.size();
}

// 再生中は開始前の編集状態を比較し、実行中の変更を未保存扱いにしない。
bool EditorScene::IsHistoryDirty(const IWorldObjects& scene) const {
	// 再生中は、再生開始前の配置で判定する。
	if (isPlaying_) {
		return !AreHistoryScenesEqual(historyPlaySnapshot_, historySavedState_);
	}

	// 編集中の配置と保存済みの配置を比較する。
	return !DoesHistoryMatchSnapshot(scene, historySavedState_);
}

bool EditorScene::IsHistoryPaused() const {
	// 一時停止の状態を返す。
	return isPaused_;
}

// 保存成功時の編集データを、未保存判定の新しい基準にする。
void EditorScene::MarkSceneSaved(const IWorldObjects& scene) {
	// 再生中の一時的な配置を保存基準にしない。
	if (!isPlaying_) {
		historySavedState_ = scene.CaptureObjects();
	}
}

void EditorScene::CollectHistoryAssetReferences(std::vector<AssetReference>& references) const {
	// Undo／Redoで復元するAssetも使用中に含める。
	for (size_t historyIndex = 0; historyIndex < history_.size(); ++historyIndex) {
		WorldAssetReferences::Collect(history_[historyIndex], "Undo/Redo " + std::to_string(historyIndex), references);
	}

	// 保存基準と停止時の復元先も調査する。
	WorldAssetReferences::Collect(historySavedState_, "Saved state", references);

	WorldAssetReferences::Collect(historyPlaySnapshot_, "Play snapshot", references);
}

void EditorScene::RemapHistoryAssetPaths(const AssetPathChange& change) {
	// 過去の履歴も移動後のAssetを参照させる。
	for (std::vector<WorldObjectData>& snapshot : history_) {
		WorldAssetReferences::Remap(snapshot, change);
	}

	// 比較基準と再生前の配置にも同じ変更を反映する。
	WorldAssetReferences::Remap(historySavedState_, change);

	WorldAssetReferences::Remap(historyPlaySnapshot_, change);
}

// 未確定の編集を記録してから、一つ前の設定を復元する。
void EditorScene::Undo(IWorldObjects& scene, std::string& error) {
	// 前回の失敗理由を消す。
	error.clear();

	// 未記録の編集を確定してから戻る。
	UpdateHistory(scene, false);

	// 巻き戻せる履歴があるか確認する。
	if (!CanHistoryUndo()) {
		error = "Nothing to undo.";

		return;
	}

	// 復元できなければ現在位置を変えない。
	if (!scene.ReplaceObjects(history_[historyIndex_ - 1], error)) {
		return;
	}

	// 復元成功後に履歴位置を戻す。
	--historyIndex_;
}

// 復元に成功した場合だけ履歴位置を進め、失敗時の状態を保つ。
void EditorScene::Redo(IWorldObjects& scene, std::string& error) {
	// 前回の失敗理由を消す。
	error.clear();

	// 未記録の編集があればRedo側の分岐を更新する。
	UpdateHistory(scene, false);

	// やり直せる履歴があるか確認する。
	if (!CanHistoryRedo()) {
		error = "Nothing to redo.";

		return;
	}

	// 復元できなければ現在位置を変えない。
	if (!scene.ReplaceObjects(history_[historyIndex_ + 1], error)) {
		return;
	}

	// 復元成功後に履歴位置を進める。
	++historyIndex_;
}

// 復元できる有効なデータであることを確認し、編集状態を退避して再生する。
bool EditorScene::BeginHistoryPlay(IWorldObjects& scene, std::string& error) {
	// 今回の再生要求の結果だけを返す。
	error.clear();

	// 再生の二重開始を防ぐ。
	if (isPlaying_) {
		error = "This scene is already playing.";

		return false;
	}

	// 停止時に配置を復元できるか確認する。
	if (!scene.HasObjectCollectionSupport()) {
		error = "This scene cannot enter play mode.";

		return false;
	}

	// 再生前の設定を値として取り出す。
	const std::vector<WorldObjectData> objects = scene.CaptureObjects();

	// 各オブジェクトの設定値を検証する。
	for (const WorldObjectData& object : objects) {
		if (!WorldObjects::IsObjectValid(object)) {
			error = "Fix invalid object values before entering play mode.";

			return false;
		}
	}

	// 親子関係や個数制限も含めて一括検証する。
	WorldObjects validator;

	if (!validator.ReplaceObjects(objects, error)) {
		return false;
	}

	// 最後の編集を履歴へ確定する。
	UpdateHistory(scene, false);

	// 停止時に戻す配置を保持する。
	historyPlaySnapshot_ = objects;

	// 一時停止していない再生状態へ切り替える。
	isPlaying_ = true;

	isPaused_ = false;

	isStepRequested_ = false;

	return true;
}

// 退避した設定の復元に成功してから、編集モードへ戻す。
bool EditorScene::StopHistory(IWorldObjects& scene, std::string& error) {
	// 今回の停止要求の結果だけを返す。
	error.clear();

	// 編集モードでの重複した停止要求を拒否する。
	if (!isPlaying_) {
		error = "The scene is not playing.";

		return false;
	}

	// 復元に失敗した場合は再生状態を維持する。
	if (!scene.ReplaceObjects(historyPlaySnapshot_, error)) {
		return false;
	}

	// 復元できたので編集モードへ戻す。
	isPlaying_ = false;

	isPaused_ = false;

	isStepRequested_ = false;

	// 復元に使った一時データを解放する。
	historyPlaySnapshot_.clear();

	return true;
}

// 一時停止の切替時に古いステップ要求を消し、意図しない更新を防ぐ。
void EditorScene::ToggleHistoryPause() {
	// 再生中だけ一時停止を切り替える。
	if (isPlaying_) {
		isPaused_ = !isPaused_;

		// 切替前のステップ要求を持ち越さない。
		isStepRequested_ = false;
	}
}

// 再生中かつ一時停止中の場合だけ、一回分のゲーム更新を予約する。
void EditorScene::RequestHistoryStep() {
	// 一時停止中だけ、一フレーム更新を予約する。
	if (isPlaying_ && isPaused_) {
		isStepRequested_ = true;
	}
}

// 編集がないフレームは、参照だけで履歴や保存済み設定と比較する。
bool EditorScene::DoesHistoryMatchSnapshot(const IWorldObjects& scene, const std::vector<WorldObjectData>& snapshot) {
	// 個数が違えば設定全体も異なる。
	if (scene.GetEditableObjectCount() != snapshot.size()) {
		return false;
	}

	// 一覧をコピーせず、各設定を順番に比較する。
	for (size_t objectIndex = 0; objectIndex < snapshot.size(); ++objectIndex) {
		const WorldObjectData* object = scene.GetObjectData(objectIndex);

		// 参照を公開しない実装では値のコピーで比較する。
		if (object == nullptr) {
			return AreHistoryScenesEqual(scene.CaptureObjects(), snapshot);
		}

		if (!AreHistoryObjectsEqual(*object, snapshot[objectIndex])) {
			return false;
		}
	}

	return true;
}

// 保存対象の全プロパティを比較する。UIの選択状態やパネル配置は比較しない。
bool EditorScene::AreHistoryObjectsEqual(const WorldObjectData& firstObject, const WorldObjectData& secondObject) {
	// 光源・物理・動作・アニメーションの設定を比較する。
	if (!AreLightSettingsEqual(firstObject.light, secondObject.light)) {
		return false;
	}

	if (!AreBodySettingsEqual(firstObject.body, secondObject.body)) {
		return false;
	}

	if (!AreColliderSettingsEqual(firstObject.collider, secondObject.collider)) {
		return false;
	}

	if (!AreMotionSettingsEqual(firstObject.motion, secondObject.motion)) {
		return false;
	}

	if (!AreAnimationSettingsEqual(firstObject.animation, secondObject.animation)) {
		return false;
	}
	// ParticleはEffect文書を正として比較し、旧互換設定の差でUndo履歴を増やさない。
	if (firstObject.type != WorldObjectType::Particle &&
		!AreParticleSettingsEqual(firstObject.particle, secondObject.particle)) {
		return false;
	}

	// Materialの個数と、各スロットの設定を比較する。
	if (firstObject.materials.size() != secondObject.materials.size()) {
		return false;
	}

	for (size_t slotIndex = 0; slotIndex < firstObject.materials.size(); ++slotIndex) {
		if (!AreMaterialSettingsEqual(firstObject.materials[slotIndex], secondObject.materials[slotIndex])) {
			return false;
		}
	}

	// Transformはコピーせず参照する。
	const TransformData& firstTransform = firstObject.transform;
	const TransformData& secondTransform = secondObject.transform;

	// ノード名・階層・有効状態・編集画面上の配置もScene Undoと未保存差分に含める。
	// 識別情報・描画設定・Transformの残りの項目を比較する。
	return firstObject.prefabSource == secondObject.prefabSource && firstObject.id == secondObject.id &&
		   firstObject.parentId == secondObject.parentId && firstObject.name == secondObject.name &&
		   firstObject.model == secondObject.model && firstObject.isActive == secondObject.isActive &&
		   firstObject.hasMeshRenderer == secondObject.hasMeshRenderer && firstObject.type == secondObject.type &&
		   firstObject.lightIntensity == secondObject.lightIntensity &&
		   firstObject.lightRadius == secondObject.lightRadius && firstObject.lightDecay == secondObject.lightDecay &&
		   firstObject.isMainCamera == secondObject.isMainCamera &&
		   firstObject.camera.fieldOfView == secondObject.camera.fieldOfView &&
		   firstObject.camera.nearClip == secondObject.camera.nearClip &&
		   firstObject.camera.farClip == secondObject.camera.farClip &&
		   AreParticleEffectDocumentsEqual(firstObject.particleEffect, secondObject.particleEffect) &&
		   firstObject.isLightingEnabled == secondObject.isLightingEnabled &&
		   firstObject.shininess == secondObject.shininess &&
		   firstObject.isRootNodeMatrixUsed == secondObject.isRootNodeMatrixUsed &&
		   firstTransform.scale.x == secondTransform.scale.x && firstTransform.scale.y == secondTransform.scale.y &&
		   firstTransform.scale.z == secondTransform.scale.z && firstTransform.rotate.x == secondTransform.rotate.x &&
		   firstTransform.rotate.y == secondTransform.rotate.y && firstTransform.rotate.z == secondTransform.rotate.z &&
		   firstTransform.translate.x == secondTransform.translate.x &&
		   firstTransform.translate.y == secondTransform.translate.y &&
		   firstTransform.translate.z == secondTransform.translate.z && firstObject.color.x == secondObject.color.x &&
		   firstObject.color.y == secondObject.color.y && firstObject.color.z == secondObject.color.z &&
		   firstObject.color.w == secondObject.color.w;
}

// オブジェクトの個数・並び順・設定を比較し、シーン全体の変更を検出する。
bool EditorScene::AreHistoryScenesEqual(const std::vector<WorldObjectData>& firstScene,
										const std::vector<WorldObjectData>& secondScene) {
	// 個数が違えばシーン全体も異なる。
	if (firstScene.size() != secondScene.size()) {
		return false;
	}

	// 並び順も含めて一件ずつ比較する。
	for (size_t objectIndex = 0; objectIndex < firstScene.size(); ++objectIndex) {
		if (!AreHistoryObjectsEqual(firstScene[objectIndex], secondScene[objectIndex])) {
			return false;
		}
	}

	return true;
}
