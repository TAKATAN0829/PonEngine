#include "../project/CG2/editor/EditorSession.h"
#include "SceneObjects.h"

#include <stdexcept>
#include <limits>

namespace {
	// 変更がない更新でスナップショットを複製しないことを計測する。
	class SnapshotCountingScene : public SceneObjects {
	public:
		std::vector<SceneObjectData> CaptureObjects() const override {
			++captureCount_;
			return SceneObjects::CaptureObjects();
		}
		size_t GetCaptureCount() const { return captureCount_; }
	private:
		mutable size_t captureCount_ = 0;
	};

	// 条件を満たさない場合は例外を投げ、テスト全体を失敗として終了させる。
	void RequireSessionCondition(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
}

// 履歴・ドラッグの集約・再生中の復元・ステップ実行を描画環境なしで検証する。
void RunEditorSessionTests() {
	SnapshotCountingScene unchangedScene;
	unchangedScene.AddObject("plane.gltf");
	EditorSession unchangedSession;
	unchangedSession.Initialize(unchangedScene);
	const size_t initialCaptureCount = unchangedScene.GetCaptureCount();
	for (size_t frameIndex = 0; frameIndex < 120; ++frameIndex) {
		unchangedSession.Update(unchangedScene, false);
		RequireSessionCondition(!unchangedSession.IsDirty(unchangedScene), "Unchanged scene became dirty");
	}
	RequireSessionCondition(unchangedScene.GetCaptureCount() == initialCaptureCount, "Unchanged frames copied scene snapshots");
	unchangedScene.GetEditableObject(0).transform->translate.x = 1;
	unchangedSession.Update(unchangedScene, false);
	RequireSessionCondition(unchangedScene.GetCaptureCount() == initialCaptureCount + 1, "Actual edit did not create one snapshot");

	SceneObjects scene;
	const SceneObjectId originalId = scene.AddObject("plane.gltf");
	EditorSession session;
	session.Initialize(scene);
	std::string error;
	RequireSessionCondition(!session.IsDirty(scene) && !session.CanUndo(), "Initial session state is incorrect");
	RequireSessionCondition(!session.ShouldSimulate(), "Edit mode is simulating");

	// ドラッグに何フレームかかっても、履歴が一件だけ記録されることを確認する。
	scene.GetEditableObject(0).transform->translate.x = 1;
	session.Update(scene, true);
	scene.GetEditableObject(0).transform->translate.x = 2;
	session.Update(scene, true);
	RequireSessionCondition(!session.CanUndo() && session.IsDirty(scene), "Drag was committed too early");
	session.Update(scene, false);
	RequireSessionCondition(session.Undo(scene, error), "Cannot undo transform drag");
	RequireSessionCondition(scene.GetEditableObject(0).transform->translate.x == 0 && !session.IsDirty(scene),
		"Undo did not restore the saved state");
	RequireSessionCondition(session.Redo(scene, error) && scene.GetEditableObject(0).transform->translate.x == 2,
		"Redo did not restore the completed drag");
	session.MarkSaved(scene);
	RequireSessionCondition(!session.IsDirty(scene), "Save did not clear dirty state");

	const SceneObjectId duplicateId = scene.DuplicateObject(originalId);
	session.Update(scene, false);
	RequireSessionCondition(session.IsDirty(scene) && session.Undo(scene, error) && scene.GetEditableObjectCount() == 1,
		"Duplicate undo failed");
	RequireSessionCondition(!session.IsDirty(scene) && session.Redo(scene, error) &&
		scene.GetEditableObject(1).id == duplicateId, "Duplicate redo lost the stable ID");

	*scene.GetEditableObject(0).editableName = "Player";
	*scene.GetEditableObject(0).active = false;
	*scene.GetEditableObject(0).hasMeshRenderer = false;
	session.Update(scene, false);
	RequireSessionCondition(session.Undo(scene, error) && *scene.GetEditableObject(0).active &&
		*scene.GetEditableObject(0).hasMeshRenderer, "Component undo failed");
	RequireSessionCondition(session.Redo(scene, error) && *scene.GetEditableObject(0).editableName == "Player" &&
		!*scene.GetEditableObject(0).hasMeshRenderer, "Component redo failed");

	// 再生中に追加・削除しても、停止時に編集用のオブジェクトと設定が復元されることを確認する。
	const std::vector<SceneObjectData> beforePlay = scene.CaptureObjects();
	RequireSessionCondition(session.BeginPlay(scene, error) && session.IsPlaying(), "Play failed");
	RequireSessionCondition(session.ShouldSimulate(), "Play mode is not simulating");
	RequireSessionCondition(!session.CanUndo() && !session.CanRedo(), "History enabled while playing");
	session.TogglePause();
	RequireSessionCondition(session.IsPaused(), "Pause failed");
	RequireSessionCondition(!session.ShouldSimulate(), "Paused scene is simulating");
	session.RequestStep();
	RequireSessionCondition(session.ShouldSimulate() && !session.ShouldSimulate(), "Step did not advance exactly once");
	session.TogglePause();
	RequireSessionCondition(!session.IsPaused(), "Resume failed");
	scene.RemoveObject(originalId);
	scene.AddObject("axis.obj");
	session.Update(scene, false);
	RequireSessionCondition(session.Stop(scene, error) && !session.IsPlaying() && !session.IsPaused(), "Stop failed");
	RequireSessionCondition(!session.ShouldSimulate(), "Stopped scene is simulating");
	const std::vector<SceneObjectData> afterPlay = scene.CaptureObjects();
	RequireSessionCondition(afterPlay.size() == beforePlay.size() && afterPlay[0].id == originalId &&
		afterPlay[0].name == "Player" && !afterPlay[0].active && !afterPlay[0].hasMeshRenderer,
		"Stop did not restore component data");
	RequireSessionCondition(session.Undo(scene, error) && *scene.GetEditableObject(0).hasMeshRenderer,
		"Play mode damaged edit-mode history");

	// Undo後の編集で古いRedo履歴が破棄されることを確認する。
	scene.AddObject("axis.obj");
	session.Update(scene, false);
	RequireSessionCondition(!session.CanRedo(), "Redo was retained after a new edit");
	RequireSessionCondition(scene.ReplaceObjects({}, error), "Cannot create empty scene");
	session.Update(scene, false);
	RequireSessionCondition(session.Undo(scene, error) && scene.GetEditableObjectCount() > 0,
		"New scene cannot be undone");

	session.Initialize(scene);
	RequireSessionCondition(!session.CanUndo() && !session.CanRedo() && !session.IsDirty(scene),
		"Load/reset retained old history");
	for (size_t editIndex = 0; editIndex < 100; ++editIndex) {
		scene.GetEditableObject(0).transform->translate.x += 1;
		session.Update(scene, false);
	}
	size_t undoCount = 0;
	while (session.CanUndo()) {
		RequireSessionCondition(session.Undo(scene, error), "Bounded history undo failed");
		++undoCount;
	}
	RequireSessionCondition(undoCount == 63, "History bound is incorrect");
	scene.GetEditableObject(0).transform->translate.x = std::numeric_limits<float>::infinity();
	RequireSessionCondition(!session.BeginPlay(scene, error) && !session.IsPlaying(),
		"Invalid object data entered play mode");

	SceneObjects cleanScene;
	cleanScene.AddObject("plane.gltf");
	session.Initialize(cleanScene);
	RequireSessionCondition(session.BeginPlay(cleanScene, error), "Clean scene cannot play");
	cleanScene.GetEditableObject(0).transform->translate.x = 99;
	cleanScene.AddObject("axis.obj");
	session.MarkSaved(cleanScene);
	RequireSessionCondition(!session.IsDirty(cleanScene), "Runtime edits changed edit-mode dirty state");
	RequireSessionCondition(session.Stop(cleanScene, error) && !session.IsDirty(cleanScene) &&
		cleanScene.GetEditableObjectCount() == 1 && cleanScene.GetEditableObject(0).transform->translate.x == 0,
		"Runtime edits were saved into the edit-mode snapshot");
}
