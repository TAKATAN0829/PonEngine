#include "EditorSession.h"
#include "objects/WorldObjects.h"

//=============================================================================================//
// 初期化処理


// 読込後のシーンを基準に、履歴・保存状態・再生状態を初期化する。
void EditorSession::Initialize(IEditableWorld& scene) {
	// 以前の履歴を破棄する。
	history_.clear();

	// 読込直後の配置を履歴の先頭にする。
	history_.push_back(scene.CaptureObjects());

	// 現在位置を最初の履歴へ戻す。
	historyIndex_ = 0;

	// 読込直後は保存済みとして扱う。
	savedState_ = history_.front();

	// 前の再生で退避した配置を消す。
	playSnapshot_.clear();

	// 編集モードから開始する。
	isPlaying_ = false;

	// 一時停止とステップ要求を解除する。
	isPaused_ = false;

	isStepRequested_ = false;
}

//=============================================================================================//
// 更新処理


// ドラッグや文字入力の途中は記録せず、完了した操作を履歴一件にまとめる。
void EditorSession::Update(IEditableWorld& scene, bool isInteractionActive) {
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
		Initialize(scene);

		return;
	}

	// 設定が同じなら履歴を増やさない。
	if (DoesMatchSnapshot(scene, history_[historyIndex_])) {
		return;
	}

	// Undo後の新しい編集では、古いRedo側の分岐を残さない。
	history_.resize(historyIndex_ + 1);

	// 確定した編集結果を一件追加する。
	history_.push_back(scene.CaptureObjects());

	if (history_.size() > kMaxHistoryStates) {
		// 古い状態から削除し、大きなシーンでも履歴が無制限に増えないようにする。
		history_.erase(history_.begin());
	}

	// 最新の編集結果を現在位置にする。
	historyIndex_ = history_.size() - 1;
}

//=============================================================================================//
// Getter / 履歴・保存状態


// 再生中の一時変更は編集履歴の対象外とし、巻き戻しを禁止する。
bool EditorSession::CanUndo() const {
	// 再生中は巻き戻さない。
	if (isPlaying_) {
		return false;
	}

	// 履歴がなければ戻れない。
	if (history_.empty()) {
		return false;
	}

	// 現在位置より前に履歴があるか確認する。
	if (historyIndex_ > 0) {
		return true;
	}

	return false;
}

// 現在位置より先に履歴が残っている場合だけ、やり直しを許可する。
bool EditorSession::CanRedo() const {
	// 再生中は編集履歴を進めない。
	if (isPlaying_) {
		return false;
	}

	// 現在位置の次に履歴が残っているか確認する。
	if (historyIndex_ + 1 < history_.size()) {
		return true;
	}

	return false;
}

// 再生中は開始前の編集状態を比較し、実行中の変更を未保存扱いにしない。
bool EditorSession::IsDirty(const IEditableWorld& scene) const {
	// 再生中は、再生開始前の配置で判定する。
	if (isPlaying_) {
		if (!AreScenesEqual(playSnapshot_, savedState_)) {
			return true;
		}

		return false;
	}

	// 編集中の配置と保存済みの配置を比較する。
	if (!DoesMatchSnapshot(scene, savedState_)) {
		return true;
	}

	return false;
}

//=============================================================================================//
// Getter / 再生状態

bool EditorSession::IsPlaying() const {
	// ゲーム更新が有効な状態か返す。
	if (isPlaying_) {
		return true;
	}

	return false;
}

bool EditorSession::IsPaused() const {
	// 一時停止の状態を返す。
	if (isPaused_) {
		return true;
	}

	return false;
}

// ステップ要求をここで消費し、次のフレームへ持ち越さない。
bool EditorSession::ShouldSimulate() {
	// 今回の更新で使うステップ要求を取り出す。
	const bool isStepRequested = isStepRequested_;

	// 同じ要求で次のフレームも更新しない。
	isStepRequested_ = false;

	// 編集モードではゲームを更新しない。
	if (!isPlaying_) {
		return false;
	}

	// 通常再生か、一回分のステップなら更新する。
	if (!isPaused_ || isStepRequested) {
		return true;
	}

	return false;
}

//=============================================================================================//
// Setter / 保存基準


// 保存成功時の編集データを、未保存判定の新しい基準にする。
void EditorSession::MarkSaved(const IEditableWorld& scene) {
	// 再生中の一時的な配置を保存基準にしない。
	if (!isPlaying_) {
		savedState_ = scene.CaptureObjects();
	}
}

//=============================================================================================//
// Asset参照

void EditorSession::CollectAssetReferences(std::vector<AssetReference>& references) const {
	// Undo／Redoで復元するAssetも使用中に含める。
	for (size_t historyIndex = 0; historyIndex < history_.size(); ++historyIndex) {
		WorldAssetReferences::Collect(history_[historyIndex], "Undo/Redo " + std::to_string(historyIndex), references);
	}

	// 保存基準と停止時の復元先も調査する。
	WorldAssetReferences::Collect(savedState_, "Saved state", references);

	WorldAssetReferences::Collect(playSnapshot_, "Play snapshot", references);
}

void EditorSession::RemapAssetPaths(const AssetPathChange& change) {
	// 過去の履歴も移動後のAssetを参照させる。
	for (std::vector<WorldObjectData>& snapshot : history_) {
		WorldAssetReferences::Remap(snapshot, change);
	}

	// 比較基準と再生前の配置にも同じ変更を反映する。
	WorldAssetReferences::Remap(savedState_, change);

	WorldAssetReferences::Remap(playSnapshot_, change);
}

//=============================================================================================//
// 履歴操作


// 未確定の編集を記録してから、一つ前の設定を復元する。
bool EditorSession::Undo(IEditableWorld& scene, std::string& error) {
	// 前回の失敗理由を消す。
	error.clear();

	// 未記録の編集を確定してから戻る。
	Update(scene, false);

	// 巻き戻せる履歴があるか確認する。
	if (!CanUndo()) {
		error = "Nothing to undo.";

		return false;
	}

	// 復元できなければ現在位置を変えない。
	if (!scene.ReplaceObjects(history_[historyIndex_ - 1], error)) {
		return false;
	}

	// 復元成功後に履歴位置を戻す。
	--historyIndex_;

	return true;
}

// 復元に成功した場合だけ履歴位置を進め、失敗時の状態を保つ。
bool EditorSession::Redo(IEditableWorld& scene, std::string& error) {
	// 前回の失敗理由を消す。
	error.clear();

	// 未記録の編集があればRedo側の分岐を更新する。
	Update(scene, false);

	// やり直せる履歴があるか確認する。
	if (!CanRedo()) {
		error = "Nothing to redo.";

		return false;
	}

	// 復元できなければ現在位置を変えない。
	if (!scene.ReplaceObjects(history_[historyIndex_ + 1], error)) {
		return false;
	}

	// 復元成功後に履歴位置を進める。
	++historyIndex_;

	return true;
}

//=============================================================================================//
// 再生操作


// 復元できる有効なデータであることを確認し、編集状態を退避して再生する。
bool EditorSession::BeginPlay(IEditableWorld& scene, std::string& error) {
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

	if (!validator.ReplaceObjects(objects,error)) {
		return false;
	}

	// 最後の編集を履歴へ確定する。
	Update(scene, false);

	// 停止時に戻す配置を保持する。
	playSnapshot_ = objects;

	// 一時停止していない再生状態へ切り替える。
	isPlaying_ = true;

	isPaused_ = false;

	isStepRequested_ = false;

	return true;
}

// 退避した設定の復元に成功してから、編集モードへ戻す。
bool EditorSession::Stop(IEditableWorld& scene, std::string& error) {
	// 今回の停止要求の結果だけを返す。
	error.clear();

	// 編集モードでの重複した停止要求を拒否する。
	if (!isPlaying_) {
		error = "The scene is not playing.";

		return false;
	}

	// 復元に失敗した場合は再生状態を維持する。
	if (!scene.ReplaceObjects(playSnapshot_, error)) {
		return false;
	}

	// 復元できたので編集モードへ戻す。
	isPlaying_ = false;

	isPaused_ = false;

	isStepRequested_ = false;

	// 復元に使った一時データを解放する。
	playSnapshot_.clear();

	return true;
}

// 一時停止の切替時に古いステップ要求を消し、意図しない更新を防ぐ。
void EditorSession::TogglePause() {
	// 再生中だけ一時停止を切り替える。
	if (isPlaying_) {
		isPaused_ = !isPaused_;

		// 切替前のステップ要求を持ち越さない。
		isStepRequested_ = false;
	}
}

// 再生中かつ一時停止中の場合だけ、一回分のゲーム更新を予約する。
void EditorSession::RequestStep() {
	// 一時停止中だけ、一フレーム更新を予約する。
	if (isPlaying_ && isPaused_) {
		isStepRequested_ = true;
	}
}

//=============================================================================================//
// 設定比較


// 編集がないフレームは、参照だけで履歴や保存済み設定と比較する。
bool EditorSession::DoesMatchSnapshot(const IEditableWorld& scene, const std::vector<WorldObjectData>& snapshot) {
	// 個数が違えば設定全体も異なる。
	if (scene.GetEditableObjectCount() != snapshot.size()) {
		return false;
	}

	// 一覧をコピーせず、各設定を順番に比較する。
	for (size_t objectIndex = 0; objectIndex < snapshot.size(); ++objectIndex) {
		const WorldObjectData* object = scene.GetObjectData(objectIndex);

		// 参照を公開しない実装では値のコピーで比較する。
		if (object == nullptr) {
			return AreScenesEqual(scene.CaptureObjects(), snapshot);
		}

		if (!AreObjectsEqual(*object, snapshot[objectIndex])) {
			return false;
		}
	}

	return true;
}

// 保存対象の全プロパティを比較する。UIの選択状態やパネル配置は比較しない。
bool EditorSession::AreObjectsEqual(const WorldObjectData& firstObject, const WorldObjectData& secondObject) {
	// 光源・物理・動作・アニメーションの設定を比較する。
	if (!AreLightSettingsEqual(firstObject.light,secondObject.light)) {
		return false;
	}

	if (!AreBodySettingsEqual(firstObject.body,secondObject.body)) {
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

	// ノード名・階層・有効状態もScene Undoと未保存差分に含める。
	// 識別情報・描画設定・Transformの残りの項目を比較する。
	return firstObject.prefabSource == secondObject.prefabSource && firstObject.id == secondObject.id && firstObject.parentId == secondObject.parentId && firstObject.name == secondObject.name &&
		firstObject.model == secondObject.model && firstObject.isActive == secondObject.isActive &&
		firstObject.hasMeshRenderer == secondObject.hasMeshRenderer &&
		firstObject.type == secondObject.type && firstObject.lightIntensity == secondObject.lightIntensity &&
		firstObject.lightRadius == secondObject.lightRadius && firstObject.lightDecay == secondObject.lightDecay &&
		firstObject.isMainCamera == secondObject.isMainCamera && firstObject.camera.fieldOfView == secondObject.camera.fieldOfView &&
		firstObject.camera.nearClip == secondObject.camera.nearClip && firstObject.camera.farClip == secondObject.camera.farClip &&
		AreParticleSettingsEqual(firstObject.particle, secondObject.particle) &&
		AreParticleEffectDocumentsEqual(firstObject.particleEffect, secondObject.particleEffect) &&
		firstObject.isLightingEnabled == secondObject.isLightingEnabled && firstObject.shininess == secondObject.shininess &&
		firstObject.isRootNodeMatrixUsed == secondObject.isRootNodeMatrixUsed &&
		firstTransform.scale.x == secondTransform.scale.x && firstTransform.scale.y == secondTransform.scale.y &&
		firstTransform.scale.z == secondTransform.scale.z && firstTransform.rotate.x == secondTransform.rotate.x &&
		firstTransform.rotate.y == secondTransform.rotate.y && firstTransform.rotate.z == secondTransform.rotate.z &&
		firstTransform.translate.x == secondTransform.translate.x && firstTransform.translate.y == secondTransform.translate.y &&
		firstTransform.translate.z == secondTransform.translate.z && firstObject.color.x == secondObject.color.x &&
		firstObject.color.y == secondObject.color.y && firstObject.color.z == secondObject.color.z &&
		firstObject.color.w == secondObject.color.w;
}

// オブジェクトの個数・並び順・設定を比較し、シーン全体の変更を検出する。
bool EditorSession::AreScenesEqual(const std::vector<WorldObjectData>& firstScene,
	const std::vector<WorldObjectData>& secondScene) {
	// 個数が違えばシーン全体も異なる。
	if (firstScene.size() != secondScene.size()) {
		return false;
	}

	// 並び順も含めて一件ずつ比較する。
	for (size_t objectIndex = 0; objectIndex < firstScene.size(); ++objectIndex) {
		if (!AreObjectsEqual(firstScene[objectIndex], secondScene[objectIndex])) {
			return false;
		}
	}

	return true;
}
