#include "EditorSession.h"
#include "SceneObjects.h"

// 保存対象の全プロパティを比較する。UIの選択状態やパネル配置は比較しない。
bool EditorSession::AreObjectsEqual(const SceneObjectData& firstObject, const SceneObjectData& secondObject) {
	const TransformData& firstTransform = firstObject.transform;
	const TransformData& secondTransform = secondObject.transform;
	return firstObject.id == secondObject.id && firstObject.parentId == secondObject.parentId && firstObject.name == secondObject.name &&
		firstObject.model == secondObject.model && firstObject.active == secondObject.active &&
		firstObject.hasMeshRenderer == secondObject.hasMeshRenderer &&
		firstObject.type == secondObject.type && firstObject.lightIntensity == secondObject.lightIntensity &&
		firstObject.lightRadius == secondObject.lightRadius && firstObject.lightDecay == secondObject.lightDecay &&
		firstObject.mainCamera == secondObject.mainCamera && firstObject.camera.fieldOfView == secondObject.camera.fieldOfView &&
		firstObject.camera.nearClip == secondObject.camera.nearClip && firstObject.camera.farClip == secondObject.camera.farClip &&
		EqualParticleSettings(firstObject.particle, secondObject.particle) &&
		firstObject.enableLighting == secondObject.enableLighting && firstObject.shininess == secondObject.shininess &&
		firstObject.useRootNodeMatrix == secondObject.useRootNodeMatrix &&
		firstTransform.scale.x == secondTransform.scale.x && firstTransform.scale.y == secondTransform.scale.y &&
		firstTransform.scale.z == secondTransform.scale.z && firstTransform.rotate.x == secondTransform.rotate.x &&
		firstTransform.rotate.y == secondTransform.rotate.y && firstTransform.rotate.z == secondTransform.rotate.z &&
		firstTransform.translate.x == secondTransform.translate.x && firstTransform.translate.y == secondTransform.translate.y &&
		firstTransform.translate.z == secondTransform.translate.z && firstObject.color.x == secondObject.color.x &&
		firstObject.color.y == secondObject.color.y && firstObject.color.z == secondObject.color.z &&
		firstObject.color.w == secondObject.color.w;
}

// オブジェクトの個数・並び順・設定を比較し、シーン全体の変更を検出する。
bool EditorSession::AreScenesEqual(const std::vector<SceneObjectData>& firstScene,
	const std::vector<SceneObjectData>& secondScene) {
	if (firstScene.size() != secondScene.size()) {
		return false;
	}
	for (size_t objectIndex = 0; objectIndex < firstScene.size(); ++objectIndex) {
		if (!AreObjectsEqual(firstScene[objectIndex], secondScene[objectIndex])) {
			return false;
		}
	}
	return true;
}

// 編集がないフレームは、参照だけで履歴や保存済み設定と比較する。
bool EditorSession::MatchesSnapshot(const IEditableScene& scene, const std::vector<SceneObjectData>& snapshot) {
	if (scene.GetEditableObjectCount() != snapshot.size()) {
		return false;
	}
	for (size_t objectIndex = 0; objectIndex < snapshot.size(); ++objectIndex) {
		const SceneObjectData* object = scene.GetObjectData(objectIndex);
		if (object == nullptr) {
			return AreScenesEqual(scene.CaptureObjects(), snapshot);
		}
		if (!AreObjectsEqual(*object, snapshot[objectIndex])) {
			return false;
		}
	}
	return true;
}

// 読込後のシーンを基準に、履歴・保存状態・再生状態を初期化する。
void EditorSession::Initialize(IEditableScene& scene) {
	history_.clear();
	history_.push_back(scene.CaptureObjects());
	historyIndex_ = 0;
	savedState_ = history_.front();
	playSnapshot_.clear();
	playing_ = false;
	paused_ = false;
	stepRequested_ = false;
}

// ドラッグや文字入力の途中は記録せず、完了した操作を履歴一件にまとめる。
void EditorSession::Update(IEditableScene& scene, bool interactionActive) {
	if (playing_) {
		return;
	}
	if (interactionActive) {
		return;
	}
	if (!scene.SupportsObjectCollection()) {
		return;
	}
	if (history_.empty()) {
		Initialize(scene);
		return;
	}
	if (MatchesSnapshot(scene, history_[historyIndex_])) {
		return;
	}
	// Undo後の新しい編集では、古いRedo側の分岐を残さない。
	history_.resize(historyIndex_ + 1);
	history_.push_back(scene.CaptureObjects());
	if (history_.size() > kMaxHistoryStates) {
		// 古い状態から削除し、大きなシーンでも履歴が無制限に増えないようにする。
		history_.erase(history_.begin());
	}
	historyIndex_ = history_.size() - 1;
}

// 再生中の一時変更は編集履歴の対象外とし、巻き戻しを禁止する。
bool EditorSession::CanUndo() const {
	if (playing_) {
		return false;
	}
	if (history_.empty()) {
		return false;
	}
	return historyIndex_ > 0;
}

// 現在位置より先に履歴が残っている場合だけ、やり直しを許可する。
bool EditorSession::CanRedo() const {
	if (playing_) {
		return false;
	}
	return historyIndex_ + 1 < history_.size();
}

// 未確定の編集を記録してから、一つ前の設定を復元する。
bool EditorSession::Undo(IEditableScene& scene, std::string& error) {
	error.clear();
	Update(scene, false);
	if (!CanUndo()) {
		error = "Nothing to undo.";
		return false;
	}
	if (!scene.ReplaceObjects(history_[historyIndex_ - 1], error)) {
		return false;
	}
	--historyIndex_;
	return true;
}

// 復元に成功した場合だけ履歴位置を進め、失敗時の状態を保つ。
bool EditorSession::Redo(IEditableScene& scene, std::string& error) {
	error.clear();
	Update(scene, false);
	if (!CanRedo()) {
		error = "Nothing to redo.";
		return false;
	}
	if (!scene.ReplaceObjects(history_[historyIndex_ + 1], error)) {
		return false;
	}
	++historyIndex_;
	return true;
}

// 再生中は開始前の編集状態を比較し、実行中の変更を未保存扱いにしない。
bool EditorSession::IsDirty(const IEditableScene& scene) const {
	if (playing_) {
		return !AreScenesEqual(playSnapshot_, savedState_);
	}
	return !MatchesSnapshot(scene, savedState_);
}

// 保存成功時の編集データを、未保存判定の新しい基準にする。
void EditorSession::MarkSaved(const IEditableScene& scene) {
	if (!playing_) {
		savedState_ = scene.CaptureObjects();
	}
}

// 復元できる有効なデータであることを確認し、編集状態を退避して再生する。
bool EditorSession::BeginPlay(IEditableScene& scene, std::string& error) {
	error.clear();
	if (playing_) {
		error = "This scene is already playing.";
		return false;
	}
	if (!scene.SupportsObjectCollection()) {
		error = "This scene cannot enter play mode.";
		return false;
	}
	const std::vector<SceneObjectData> objects = scene.CaptureObjects();
	for (const SceneObjectData& object : objects) {
		if (!SceneObjects::Validate(object)) {
			error = "Fix invalid object values before entering play mode.";
			return false;
		}
	}
	Update(scene, false);
	playSnapshot_ = objects;
	playing_ = true;
	paused_ = false;
	stepRequested_ = false;
	return true;
}

// 退避した設定の復元に成功してから、編集モードへ戻す。
bool EditorSession::Stop(IEditableScene& scene, std::string& error) {
	error.clear();
	if (!playing_) {
		error = "The scene is not playing.";
		return false;
	}
	if (!scene.ReplaceObjects(playSnapshot_, error)) {
		return false;
	}
	playing_ = false;
	paused_ = false;
	stepRequested_ = false;
	playSnapshot_.clear();
	return true;
}

// 一時停止の切替時に古いステップ要求を消し、意図しない更新を防ぐ。
void EditorSession::TogglePause() {
	if (playing_) {
		paused_ = !paused_;
		stepRequested_ = false;
	}
}

// 再生中かつ一時停止中の場合だけ、一回分のゲーム更新を予約する。
void EditorSession::RequestStep() {
	if (playing_ && paused_) {
		stepRequested_ = true;
	}
}

// ステップ要求をここで消費し、次のフレームへ持ち越さない。
bool EditorSession::ShouldSimulate() {
	const bool stepRequested = stepRequested_;
	stepRequested_ = false;
	if (!playing_) {
		return false;
	}
	return !paused_ || stepRequested;
}
