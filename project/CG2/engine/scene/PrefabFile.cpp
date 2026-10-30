#include "PrefabFile.h"
#include "SceneFile.h"
#include "SceneObjects.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>

std::filesystem::path PrefabFile::ResolveSource(const std::string& source, std::string& error) {
	error.clear();
	if (source.find('\0') != std::string::npos) {
		error = "Invalid prefab source path.";
		return {};
	}
	const std::filesystem::path relative(std::u8string(source.begin(), source.end()));
	if (source.empty() || relative.is_absolute() || relative.has_root_name() || relative.extension() != ".ponprefab") {
		error = "Invalid prefab source path.";
		return {};
	}
	for (const std::filesystem::path& part : relative) {
		if (part == "..") {
			error = "Prefab source must remain inside resources.";
			return {};
		}
	}
	std::error_code pathError;
	const std::filesystem::path resources = std::filesystem::weakly_canonical("resources", pathError);
	if (pathError) {
		error = pathError.message();
		return {};
	}
	const std::filesystem::path path = std::filesystem::weakly_canonical(resources / relative, pathError);
	if (pathError) {
		error = pathError.message();
		return {};
	}
	const std::filesystem::path contained = path.lexically_relative(resources);
	if (contained.empty()) {
		error = "Cannot resolve prefab source.";
		return {};
	}
	for (const std::filesystem::path& part : contained) {
		if (part == "..") {
			error = "Prefab source escapes resources.";
			return {};
		}
	}
	return path;
}

bool PrefabFile::Apply(IEditableScene& scene, SceneObjectId rootId, std::string& error) {
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const SceneObjectData* object = scene.GetObjectData(objectIndex);
		if (object == nullptr) {
			continue;
		}
		if (object->id == rootId) {
			const std::filesystem::path path = ResolveSource(object->prefabSource, error);
			if (path.empty()) {
				return false;
			}
			return Save(scene, rootId, path, error);
		}
	}
	error = "Prefab root does not exist.";
	return false;
}

// 一時シーンで子孫を入れ替え、リソース検証にも成功した時だけ本体へ反映する。
bool PrefabFile::Revert(IEditableScene& scene, SceneObjectId rootId, std::string& error) {
	SceneObjects candidate;
	if (!candidate.ReplaceObjects(scene.CaptureObjects(), error)) {
		return false;
	}
	SceneObjectData original;
	bool found = false;
	for (const SceneObjectData& object : candidate.GetObjects()) {
		if (object.id == rootId) {
			original = object;
			found = true;
			break;
		}
	}
	if (!found) {
		error = "Prefab root does not exist.";
		return false;
	}
	const std::filesystem::path path = ResolveSource(original.prefabSource, error);
	if (path.empty()) {
		return false;
	}
	candidate.RemoveObject(rootId);
	const SceneObjectId importedRoot = Instantiate(candidate, path, 0, {}, error);
	if (importedRoot == 0) {
		return false;
	}
	std::vector<SceneObjectData> objects = candidate.CaptureObjects();
	for (SceneObjectData& object : objects) {
		if (object.parentId == importedRoot) {
			object.parentId = rootId;
		}
		if (object.id == importedRoot) {
			object.id = rootId;
			object.parentId = original.parentId;
			object.transform = original.transform;
			object.prefabSource = original.prefabSource;
			// インスタンスのメイン指定は他のカメラと競合させない。
			object.mainCamera = original.mainCamera;
			if (object.type != SceneObjectType::Camera) {
				object.mainCamera = false;
			}
		}
	}
	return scene.ReplaceObjects(std::move(objects), error);
}

bool PrefabFile::Save(IEditableScene& scene, SceneObjectId rootId, const std::filesystem::path& path, std::string& error) {
	error.clear();
	std::vector<SceneObjectData> source = scene.CaptureObjects();
	std::unordered_set<SceneObjectId> included = {rootId};
	// 一覧の並び順に依存せず、選択したルートの子孫だけを抽出する。
	bool changed = true;
	while (changed) {
		changed = false;
		for (const SceneObjectData& object : source) {
			if (included.contains(object.parentId)) {
				if (included.insert(object.id).second) {
					changed = true;
				}
			}
		}
	}
	std::vector<SceneObjectData> subtree;
	bool foundRoot = false;
	for (SceneObjectData& object : source) {
		if (!included.contains(object.id)) {
			continue;
		}
		if (object.id == rootId) {
			object.parentId = 0;
			foundRoot = true;
		}
		// 保存先のPrefabに別のAssetへのリンクを埋め込まない。
		object.prefabSource.clear();
		subtree.push_back(std::move(object));
	}
	if (!foundRoot) {
		error = "Prefab root does not exist.";
		return false;
	}
	SceneObjects prefab;
	if (!prefab.ReplaceObjects(std::move(subtree), error)) {
		return false;
	}
	Matrix4x4 world{};
	if (!scene.GetObjectWorldMatrix(rootId, world)) {
		error = "Cannot resolve prefab root transform.";
		return false;
	}
	// 配置時の位置を原点に揃え、親から受けていた回転と拡縮は維持する。
	world.m[3][0] = 0;
	world.m[3][1] = 0;
	world.m[3][2] = 0;
	if (!prefab.SetObjectWorldTransform(rootId, world)) {
		error = "Prefab root contains shear and cannot be represented by TRS.";
		return false;
	}
	return SceneFile::Save(prefab, path, error);
}

SceneObjectId PrefabFile::Instantiate(IEditableScene& scene, const std::filesystem::path& path,
	SceneObjectId parentId, const Vector3& position, std::string& error) {
	error.clear();
	SceneObjects prefab;
	if (!SceneFile::Load(prefab, path, error)) {
		return 0;
	}
	std::vector<SceneObjectData> candidate = scene.CaptureObjects();
	std::vector<SceneObjectData> imported = prefab.CaptureObjects();
	if (imported.empty() || candidate.size() + imported.size() > SceneObjects::kMaxObjects) {
		error = "Prefab is empty or exceeds the scene object limit.";
		return 0;
	}
	SceneObjectId nextId = scene.GetNextObjectId();
	for (const SceneObjectData& object : candidate) {
		if (object.id >= nextId) {
			nextId = object.id + 1;
		}
	}
	if (nextId >= (std::numeric_limits<SceneObjectId>::max)() - imported.size()) {
		error = "Prefab object IDs exceed the supported range.";
		return 0;
	}
	std::unordered_map<SceneObjectId, SceneObjectId> remapped;
	SceneObjectId rootId = 0;
	for (const SceneObjectData& object : imported) {
		remapped[object.id] = nextId++;
		if (object.parentId == 0) {
			if (rootId != 0) {
				error = "Prefab must contain exactly one root.";
				return 0;
			}
			rootId = remapped.at(object.id);
		}
	}
	if (rootId == 0) {
		error = "Prefab root is missing.";
		return 0;
	}
	for (SceneObjectData& object : imported) {
		object.id = remapped.at(object.id);
		object.prefabSource.clear();
		if (object.id == rootId) {
			// テスト等でresources外から直接読み込む場合は独立配置のままにする。
			std::error_code pathError;
			const std::filesystem::path relative = std::filesystem::relative(path, "resources", pathError);
			if (!pathError) {
				const std::u8string encoded = relative.generic_u8string();
				const std::string source(encoded.begin(), encoded.end());
				std::string sourceError;
				if (!ResolveSource(source, sourceError).empty()) {
					object.prefabSource = source;
				}
			}
		}
		if (object.parentId != 0) {
			object.parentId = remapped.at(object.parentId);
		}
		if (scene.GetMainCameraId() != 0) {
			object.mainCamera = false;
		}
		candidate.push_back(std::move(object));
	}
	// 親座標への変換も一時シーン内で済ませ、特異な親でも元シーンを維持する。
	SceneObjects validator;
	if (!validator.ReplaceObjects(std::move(candidate), error)) {
		return 0;
	}
	if (!validator.SetObjectWorldPosition(rootId, position)) {
		error = "Invalid prefab placement position.";
		return 0;
	}
	if (!validator.SetObjectParent(rootId, parentId, error)) {
		return 0;
	}
	if (!scene.ReplaceObjects(validator.CaptureObjects(), error)) {
		return 0;
	}
	return rootId;
}
