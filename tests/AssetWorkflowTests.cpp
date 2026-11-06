#include "prefab/PrefabOperations.h"
#include "serialization/SceneFile.h"
#include "objects/WorldObjects.h"
#include "../project/CG2/editor/state/EditorSession.h"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace {
	// 実行側がGPU準備などの理由で置換を拒否する場合を再現する。
	class RejectingWorldObjects : public WorldObjects {
	public:
		bool ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error) override;

		size_t replacementCount = 0;
	};

	bool RejectingWorldObjects::ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error) {
		// Prefab操作が仮想関数を経由して最終反映することを記録する。
		(void)objects;

		++replacementCount;

		error = "Replacement rejected by runtime";

		return false;
	}

	void RequireAsset(bool isConditionSatisfied, const char* message) {
		if (!isConditionSatisfied) {
			throw std::runtime_error(message);
		}
	}
}

void RunAssetWorkflowTests(const std::filesystem::path& directory) {
	WorldObjects source;
	const WorldObjectId parentId = source.CreateObject(WorldObjectType::Empty);
	const WorldObjectId modelId = source.AddObject("plane.gltf");
	const WorldObjectId lightId = source.CreateObject(WorldObjectType::PointLight);
	std::string error;
	// ルート・モデル・ライトの階層を作り、Prefabが複数段の親子関係を保つか確認する。
	RequireAsset(source.SetObjectParent(modelId, parentId, error), "Prefab model parenting failed");
	RequireAsset(source.SetObjectParent(lightId, modelId, error), "Prefab light parenting failed");
	source.GetEditableObject(1).materials->push_back({0, "white1x1.png", {0.3f, 0.6f, 0.9f, 1}, 85, false});
	*source.GetEditableObject(1).animation = {2, false, true, 1.75f, 0.25f};
	const std::filesystem::path scenePath = directory / "asset-settings.ponscene";
	RequireAsset(SceneFile::Save(source, scenePath, error), "Asset settings save failed");
	WorldObjects restored;
	RequireAsset(SceneFile::Load(restored, scenePath, error), "Asset settings load failed");
	RequireAsset(AreMaterialSettingsEqual(restored.GetObjectData(1)->materials.front(), source.GetObjectData(1)->materials.front()), "Material settings not restored");
	RequireAsset(AreAnimationSettingsEqual(restored.GetObjectData(1)->animation, source.GetObjectData(1)->animation), "Animation settings not restored");

	EditorSession session;
	session.Initialize(restored);
	restored.GetEditableObject(1).materials->front().color.x = 0.8f;
	restored.GetEditableObject(1).animation->speed = 0.5f;
	session.Update(restored, false);
	RequireAsset(session.Undo(restored, error), "Asset settings undo failed");
	RequireAsset(restored.GetObjectData(1)->materials.front().color.x == 0.3f && restored.GetObjectData(1)->animation.speed == 1.75f, "Asset settings missing from history");
	RequireAsset(session.Redo(restored, error), "Asset settings redo failed");

	const std::filesystem::path prefabPath = directory / "nested.ponprefab";
	RequireAsset(PrefabOperations::Save(source, parentId, prefabPath, error), "Prefab save failed");
	WorldObjects destination;
	const WorldObjectId existingId = destination.CreateObject(WorldObjectType::Empty);
	session.Initialize(destination);
	const WorldObjectId firstRoot = PrefabOperations::Instantiate(destination, prefabPath, existingId, {10, 2, 3}, error);
	RequireAsset(firstRoot != 0, "Prefab instantiate failed");
	RequireAsset(destination.GetEditableObjectCount() == 4, "Prefab subtree not fully imported");
	RequireAsset(destination.GetObjectData(1)->parentId == existingId && destination.GetObjectData(2)->parentId == firstRoot, "Prefab IDs were not remapped");
	RequireAsset(destination.GetObjectData(3)->parentId == destination.GetObjectData(2)->id, "Prefab nested parent was lost");
	RequireAsset(std::abs(destination.GetObjectWorldPosition(firstRoot).x - 10) < 0.001f, "Prefab placement is incorrect");
	RequireAsset(destination.GetObjectData(2)->materials.front().shininess == 85 && destination.GetObjectData(2)->animation.clip == 2, "Prefab components not preserved");
	session.Update(destination, false);
	RequireAsset(session.Undo(destination, error) && destination.GetEditableObjectCount() == 1, "Prefab undo did not remove entire subtree");
	RequireAsset(session.Redo(destination, error) && destination.GetEditableObjectCount() == 4, "Prefab redo did not restore subtree");
	const WorldObjectId secondRoot = PrefabOperations::Instantiate(destination, prefabPath, 0, {}, error);
	RequireAsset(secondRoot > firstRoot && destination.GetEditableObjectCount() == 7, "Repeated prefab reused object IDs");
	destination.GetEditableObject(2).materials->front().color.x = 0.1f;
	RequireAsset(destination.GetObjectData(5)->materials.front().color.x == 0.3f, "Prefab instances share mutable settings");
	RequireAsset(PrefabOperations::Instantiate(destination, prefabPath, 999999, {}, error) == 0, "Missing prefab parent accepted");
	RequireAsset(destination.GetEditableObjectCount() == 7, "Failed prefab placement partially changed scene");

	// 最終反映を拒否されても、既存データと次のIDを変更しない。
	RejectingWorldObjects rejectingDestination;
	const WorldObjectId preservedId = rejectingDestination.CreateObject(WorldObjectType::Empty);
	IWorldObjects& destinationInterface = rejectingDestination;

	RequireAsset(PrefabOperations::Instantiate(destinationInterface, prefabPath, 0, {}, error) == 0, "Runtime rejection was bypassed");
	RequireAsset(rejectingDestination.replacementCount == 1, "Runtime replacement was not called exactly once");
	RequireAsset(rejectingDestination.GetEditableObjectCount() == 1, "Rejected placement changed existing objects");
	RequireAsset(rejectingDestination.GetObjectData(0)->id == preservedId, "Rejected placement changed existing ID");
	RequireAsset(rejectingDestination.CreateObject(WorldObjectType::Empty) == preservedId + 1, "Rejected placement consumed IDs");

	RequireAsset(!PrefabOperations::Save(source, 999999, directory / "invalid.ponprefab", error), "Missing prefab root accepted");
	std::vector<WorldObjectData> invalid = source.CaptureObjects();
	invalid[1].materials.push_back(invalid[1].materials.front());
	RequireAsset(!restored.ReplaceObjects(invalid, error), "Duplicate material slots accepted");
	invalid = source.CaptureObjects();
	invalid[1].materials.front().texture = "../escape.png";
	RequireAsset(!restored.ReplaceObjects(invalid, error), "Outside texture path accepted");
	invalid = source.CaptureObjects();
	invalid[1].animation.speed = -1;
	RequireAsset(!restored.ReplaceObjects(invalid, error), "Invalid animation speed accepted");
}
