#include "EditorSelection.h"
#include "SceneObjects.h"
#include <limits>
#include <unordered_map>

namespace {
	// 選択ルートとその子孫を一度だけ列挙する。一括操作中は所有スナップショットを使う。
	std::unordered_set<SceneObjectId> CollectSubtrees(const std::vector<SceneObjectData>& objects,
		const std::vector<SceneObjectId>& roots) {
		std::unordered_map<SceneObjectId, std::vector<SceneObjectId>> children;
		for (const SceneObjectData& object : objects) {
			children[object.parentId].push_back(object.id);
		}
		std::unordered_set<SceneObjectId> included;
		std::vector<SceneObjectId> pending = roots;
		while (!pending.empty()) {
			const SceneObjectId identifier = pending.back();
			pending.pop_back();
			if (!included.insert(identifier).second) {
				continue;
			}
			const std::unordered_map<SceneObjectId, std::vector<SceneObjectId>>::const_iterator found = children.find(identifier);
			if (found != children.end()) {
				pending.insert(pending.end(), found->second.begin(), found->second.end());
			}
		}
		return included;
	}
}

// 選択数分のシーン全体コピーを避け、採番・親の付け替え・検証を一回で行う。
bool EditorSelection::Duplicate(IEditableScene& scene, std::string& error) {
	error.clear();
	const std::vector<SceneObjectId> roots = GetRoots(scene);
	if (roots.empty()) {
		return false;
	}
	const std::vector<SceneObjectData> original = scene.CaptureObjects();
	const std::unordered_set<SceneObjectId> included = CollectSubtrees(original, roots);
	if (original.size() + included.size() > SceneObjects::kMaxObjects) {
		error = "Cannot duplicate selection: object limit reached.";
		return false;
	}
	SceneObjectId nextIdentifier = scene.GetNextObjectId();
	if (included.size() > (std::numeric_limits<SceneObjectId>::max)() - nextIdentifier) {
		error = "Cannot duplicate selection: ID limit reached.";
		return false;
	}
	std::unordered_map<SceneObjectId, SceneObjectId> remapped;
	for (const SceneObjectData& object : original) {
		if (included.contains(object.id)) {
			remapped[object.id] = nextIdentifier++;
		}
	}
	std::vector<SceneObjectData> candidate = original;
	candidate.reserve(original.size() + included.size());
	for (const SceneObjectData& object : original) {
		if (!included.contains(object.id)) {
			continue;
		}
		SceneObjectData copy = object;
		copy.id = remapped.at(object.id);
		if (remapped.contains(copy.parentId)) {
			copy.parentId = remapped.at(copy.parentId);
		}
		copy.mainCamera = false;
		if (copy.name.size() > 200) {
			copy.name = "Object";
		}
		copy.name += " Copy #" + std::to_string(copy.id);
		candidate.push_back(std::move(copy));
	}
	if (!scene.ReplaceObjects(std::move(candidate), error)) {
		return false;
	}
	Clear();
	for (SceneObjectId identifier : roots) {
		selected_.push_back(remapped.at(identifier));
		selectedIds_.insert(remapped.at(identifier));
	}
	return true;
}

bool EditorSelection::Remove(IEditableScene& scene, std::string& error) {
	error.clear();
	const std::vector<SceneObjectId> roots = GetRoots(scene);
	if (roots.empty()) {
		return false;
	}
	std::vector<SceneObjectData> original = scene.CaptureObjects();
	const std::unordered_set<SceneObjectId> included = CollectSubtrees(original, roots);
	std::vector<SceneObjectData> remaining;
	remaining.reserve(original.size());
	for (SceneObjectData& object : original) {
		if (!included.contains(object.id)) {
			remaining.push_back(std::move(object));
		}
	}
	if (!scene.ReplaceObjects(std::move(remaining), error)) {
		return false;
	}
	Clear();
	return true;
}
