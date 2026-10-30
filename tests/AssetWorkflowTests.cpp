#include "PrefabFile.h"
#include "SceneFile.h"
#include "SceneObjects.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace {
	void RequireAsset(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
}

void RunAssetWorkflowTests(const std::filesystem::path& directory) {
	SceneObjects source;
	const SceneObjectId parentId = source.CreateObject(SceneObjectType::Empty);
	const SceneObjectId modelId = source.AddObject("plane.gltf");
	const SceneObjectId lightId = source.CreateObject(SceneObjectType::PointLight);
	std::string error;
	RequireAsset(source.SetObjectParent(modelId, parentId, error), "Prefab model parenting failed");
	RequireAsset(source.SetObjectParent(lightId, modelId, error), "Prefab light parenting failed");
	source.GetEditableObject(1).materials->push_back({0, "white1x1.png", {0.3f, 0.6f, 0.9f, 1}, 85, false});
	*source.GetEditableObject(1).animation = {2, false, true, 1.75f, 0.25f};
	const std::filesystem::path scenePath = directory / "asset-settings.ponscene";
	RequireAsset(SceneFile::Save(source, scenePath, error), "Asset settings save failed");
	SceneObjects restored;
	RequireAsset(SceneFile::Load(restored, scenePath, error), "Asset settings load failed");
	RequireAsset(EqualMaterialSettings(restored.GetObjectData(1)->materials.front(), source.GetObjectData(1)->materials.front()), "Material settings not restored");
	RequireAsset(EqualAnimationSettings(restored.GetObjectData(1)->animation, source.GetObjectData(1)->animation), "Animation settings not restored");

	EditorSession session;
	session.Initialize(restored);
	restored.GetEditableObject(1).materials->front().color.x = 0.8f;
	restored.GetEditableObject(1).animation->speed = 0.5f;
	session.Update(restored, false);
	RequireAsset(session.Undo(restored, error), "Asset settings undo failed");
	RequireAsset(restored.GetObjectData(1)->materials.front().color.x == 0.3f && restored.GetObjectData(1)->animation.speed == 1.75f, "Asset settings missing from history");
	RequireAsset(session.Redo(restored, error), "Asset settings redo failed");

	const std::filesystem::path prefabPath = directory / "nested.ponprefab";
	RequireAsset(PrefabFile::Save(source, parentId, prefabPath, error), "Prefab save failed");
	SceneObjects destination;
	const SceneObjectId existingId = destination.CreateObject(SceneObjectType::Empty);
	session.Initialize(destination);
	const SceneObjectId firstRoot = PrefabFile::Instantiate(destination, prefabPath, existingId, {10, 2, 3}, error);
	RequireAsset(firstRoot != 0, "Prefab instantiate failed");
	RequireAsset(destination.GetEditableObjectCount() == 4, "Prefab subtree not fully imported");
	RequireAsset(destination.GetObjectData(1)->parentId == existingId && destination.GetObjectData(2)->parentId == firstRoot, "Prefab IDs were not remapped");
	RequireAsset(destination.GetObjectData(3)->parentId == destination.GetObjectData(2)->id, "Prefab nested parent was lost");
	RequireAsset(std::abs(destination.GetObjectWorldPosition(firstRoot).x - 10) < 0.001f, "Prefab placement is incorrect");
	RequireAsset(destination.GetObjectData(2)->materials.front().shininess == 85 && destination.GetObjectData(2)->animation.clip == 2, "Prefab components not preserved");
	session.Update(destination, false);
	RequireAsset(session.Undo(destination, error) && destination.GetEditableObjectCount() == 1, "Prefab undo did not remove entire subtree");
	RequireAsset(session.Redo(destination, error) && destination.GetEditableObjectCount() == 4, "Prefab redo did not restore subtree");
	const SceneObjectId secondRoot = PrefabFile::Instantiate(destination, prefabPath, 0, {}, error);
	RequireAsset(secondRoot > firstRoot && destination.GetEditableObjectCount() == 7, "Repeated prefab reused object IDs");
	destination.GetEditableObject(2).materials->front().color.x = 0.1f;
	RequireAsset(destination.GetObjectData(5)->materials.front().color.x == 0.3f, "Prefab instances share mutable settings");
	RequireAsset(PrefabFile::Instantiate(destination, prefabPath, 999999, {}, error) == 0, "Missing prefab parent accepted");
	RequireAsset(destination.GetEditableObjectCount() == 7, "Failed prefab placement partially changed scene");
	RequireAsset(!PrefabFile::Save(source, 999999, directory / "invalid.ponprefab", error), "Missing prefab root accepted");
	std::vector<SceneObjectData> invalid = source.CaptureObjects();
	invalid[1].materials.push_back(invalid[1].materials.front());
	RequireAsset(!restored.ReplaceObjects(invalid, error), "Duplicate material slots accepted");
	invalid = source.CaptureObjects();
	invalid[1].materials.front().texture = "../escape.png";
	RequireAsset(!restored.ReplaceObjects(invalid, error), "Outside texture path accepted");
	invalid = source.CaptureObjects();
	invalid[1].animation.speed = -1;
	RequireAsset(!restored.ReplaceObjects(invalid, error), "Invalid animation speed accepted");
}
