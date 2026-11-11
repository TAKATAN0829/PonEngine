#pragma once

#include "AssetPathChange.h"
#include "IEditableWorld.h"
#include "IWorldObjects.h"
#include <filesystem>
#include <string>
#include <unordered_set>
#include <vector>

// シーンの選択・履歴・保存状態。
class EditorScene {
	friend class EditorPanels;

private:
	static constexpr size_t kHistoryMaxHistoryStates = 64;

public:
	void Initialize(IEditableWorld* world, const std::string& initialPath = {});
	bool IsPlaying() const;
	bool ShouldSimulate();
	IEditableWorld& GetEditWorld(IEditableWorld& scene) const;
	void RemoveSelection(IWorldObjects& objects, std::string& error);
	void DuplicateSelection(IWorldObjects& objects, std::string& error);
	void PruneSelection(const IWorldObjects& objects);
	void ClearSelection();
	void ToggleObjectSelection(WorldObjectId identifier);
	void SelectObject(WorldObjectId identifier);
	std::vector<WorldObjectId> GetSelectionRoots(const IWorldObjects& objects) const;
	bool IsObjectSelected(WorldObjectId identifier) const;
	const std::vector<WorldObjectId>& GetSelectedObjectIds() const;
	WorldObjectId GetPrimarySelectedObjectId() const;
	static bool AreHistoryScenesEqual(const std::vector<WorldObjectData>& firstScene,
									  const std::vector<WorldObjectData>& secondScene);
	static bool AreHistoryObjectsEqual(const WorldObjectData& firstObject, const WorldObjectData& secondObject);
	static bool DoesHistoryMatchSnapshot(const IWorldObjects& objects, const std::vector<WorldObjectData>& snapshot);
	void RequestHistoryStep();
	void ToggleHistoryPause();
	bool StopHistory(IWorldObjects& objects, std::string& error);
	bool BeginHistoryPlay(IWorldObjects& objects, std::string& error);
	void Redo(IWorldObjects& objects, std::string& error);
	void Undo(IWorldObjects& objects, std::string& error);
	void RemapHistoryAssetPaths(const AssetPathChange& change);
	void CollectHistoryAssetReferences(std::vector<AssetReference>& references) const;
	void MarkSceneSaved(const IWorldObjects& objects);
	bool IsHistoryPaused() const;
	bool IsHistoryDirty(const IWorldObjects& objects) const;
	bool CanHistoryRedo() const;
	bool CanHistoryUndo() const;
	void UpdateHistory(IWorldObjects& objects, bool isInteractionActive);
	void InitializeHistory(IWorldObjects& objects);
	void RemapSceneDocumentAssetPaths(const AssetPathChange& change);
	bool SceneDocumentResolvePath(const std::filesystem::path& requested, bool mustExist,
								  std::filesystem::path& resolved, std::string& error) const;
	bool SaveSceneDocumentAs(IWorldObjects& objects, const std::filesystem::path& path, bool isOverwriteConfirmed,
							 std::string& error);
	bool SaveSceneDocument(IWorldObjects& objects, std::string& error);
	bool OpenSceneDocument(IWorldObjects& objects, IEditorSceneLifecycle& lifecycle, const std::filesystem::path& path,
						   std::string& error);
	bool SceneDocumentNew(IWorldObjects& objects, IEditorSceneLifecycle& lifecycle, std::string& error);
	std::string GetSceneDocumentName() const;
	bool HasSceneDocumentPath() const;
	const std::filesystem::path& GetSceneDocumentPath() const;
	const std::filesystem::path& GetSceneDocumentRoot() const;
	void InitializeSceneDocument(const std::filesystem::path& resources, const std::filesystem::path& initialPath,
								 std::string& error);

private:
	IEditableWorld* world_ = nullptr;
	IEditableWorld* playOrigin_ = nullptr; // SceneManagerが停止まで生存させる、再生開始時の編集対象。
	std::string sceneName_ = "GameScene";  // 現在の場面名を所有し、シーン破棄後の文字列を参照しない。
	std::string fileStatus_;			   // 保存・読込の結果。
	std::vector<WorldObjectId> selectedObjectIds_;
	std::unordered_set<WorldObjectId> selectedObjectIdSet_;
	std::vector<std::vector<WorldObjectData>> history_;
	size_t historyIndex_ = 0;
	std::vector<WorldObjectData> historySavedState_;
	std::vector<WorldObjectData> historyPlaySnapshot_;
	bool isPlaying_ = false;
	bool isPaused_ = false;
	bool isStepRequested_ = false;
	std::filesystem::path sceneDocumentRoot_;
	std::filesystem::path sceneDocumentPath_;
};
