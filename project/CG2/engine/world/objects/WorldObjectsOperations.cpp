#include "objects/WorldObjects.h"
#include "objects/WorldAssetReferences.h"
#include "settings/LightSettings.h"
#include <limits>
#include <unordered_set>

//=============================================================================================//
// Asset参照の更新

void WorldObjects::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	WorldAssetReferences::Remap(objects_, change);
}


//=============================================================================================//
// 追加・複製・削除

// 新しいIDと既定の設定を持つモデルを追加する。削除済みIDは再利用しない。
WorldObjectId WorldObjects::AddObject(const std::string& model) {
	// モデルの種類と作成可否を確認して、新しいオブジェクトを追加する。
	if (!IsSupportedModel(model)) {
		return kInvalidWorldObjectId;
	}

	if (objects_.size() >= kMaxObjects) {
		return kInvalidWorldObjectId;
	}

	if (nextId_ == (std::numeric_limits<WorldObjectId>::max)()) {
		return kInvalidWorldObjectId;
	}

	std::string importError;

	if (!PrepareModel(model, importError)) {
		return kInvalidWorldObjectId;
	}

	WorldObjectData object;

	object.id = nextId_;
	object.model = model;
	object.name = model + " #" + std::to_string(object.id);

	objects_.push_back(std::move(object));

	objectIndices_[nextId_] = objects_.size() - 1;

	return nextId_++;
}

// GPUリソースを作らず、種類に応じた編集用データだけを追加する。
WorldObjectId WorldObjects::CreateObject(WorldObjectType type) {
	// 種類に応じた初期設定を作り、一覧へ追加する。
	if (type < WorldObjectType::Model || type > WorldObjectType::DirectionalLight) {
		return kInvalidWorldObjectId;
	}

	if (!CanCreateType(type)) {
		return kInvalidWorldObjectId;
	}

	const WorldObjectId createdId = AddObject("plane.gltf");

	if (createdId == kInvalidWorldObjectId) {
		return createdId;
	}

	WorldObjectData& object = objects_.back();

	object.type = type;
	object.hasMeshRenderer = type == WorldObjectType::Model;

	std::string typeName = "Model";

	if (type == WorldObjectType::Empty) {
		typeName = "Empty";
	}

	if (type == WorldObjectType::PointLight) {
		typeName = "Point Light";

		object.transform.translate.y = 2;
	}

	if (type == WorldObjectType::Particle) {
		typeName = "Particle";
	}

	if (type == WorldObjectType::SpotLight || type == WorldObjectType::DirectionalLight) {
		typeName = LightTypes::GetName(type);

		object.transform.translate.y = 2;
		object.transform.rotate.x = 1.57079632679f;
	}

	if (type == WorldObjectType::Camera) {
		typeName = "Camera";

		object.transform.translate = {0,0,-30};
		object.isMainCamera = GetMainCameraId() == kInvalidWorldObjectId;
	}

	object.name = typeName + " #" + std::to_string(createdId);

	return createdId;
}

// 設定を値として複製し、新しいIDを付けて元のモデルから独立させる。
WorldObjectId WorldObjects::DuplicateObject(WorldObjectId id) {
	if (FindObjectIndex(id) == objects_.size()) {
		return kInvalidWorldObjectId;
	}

	std::vector<WorldObjectData> copies;

	std::unordered_map<WorldObjectId, WorldObjectId> newIds;

	WorldObjectId nextId = nextId_;

	for (const WorldObjectData& object : objects_) {
		if (!IsDescendantOf(object.id, id)) {
			continue;
		}

		if (nextId == (std::numeric_limits<WorldObjectId>::max)()) {
			return kInvalidWorldObjectId;
		}

		copies.push_back(object);

		newIds[object.id] = nextId++;
	}

	std::vector<WorldObjectData> replaced = objects_;

	for (WorldObjectData& copy : copies) {
		copy.id = newIds.at(copy.id);

		copy.isMainCamera = false;

		// 複製でメイン指定を増やさない。
		if (copy.parentId != kInvalidWorldObjectId) {
			if (newIds.contains(copy.parentId)) {
				copy.parentId = newIds.at(copy.parentId);
			}
		}

		if (copy.name.size() > 200) {
			copy.name = "Object";
		}

		copy.name += " Copy #" + std::to_string(copy.id);

		replaced.push_back(std::move(copy));
	}

	std::string error;

	if (!ReplaceObjects(std::move(replaced), error)) {
		return kInvalidWorldObjectId;
	}

	return newIds.at(id);
}

// 指定IDと子孫をまとめて削除し、残るオブジェクトのIDは変更しない。
bool WorldObjects::RemoveObject(WorldObjectId id) {
	// 対象とその子孫を除いた一覧を作り、まとめて削除する。
	if (FindObjectIndex(id) == objects_.size()) {
		return false;
	}

	std::unordered_set<WorldObjectId> removedIds;

	for (const WorldObjectData& object : objects_) {
		if (IsDescendantOf(object.id, id)) {
			removedIds.insert(object.id);
		}
	}

	std::vector<WorldObjectData>::iterator objectIterator = objects_.begin();

	while (objectIterator != objects_.end()) {
		if (removedIds.contains(objectIterator->id)) {
			objectIterator = objects_.erase(objectIterator);
		}
		else {
			++objectIterator;
		}
	}

	RebuildObjectIndices();

	return true;
}

//=============================================================================================//
// 一括置換

// 全件の検証が済んでから一覧を置き換え、不正データの部分反映を防ぐ。
bool WorldObjects::ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error) {
	// 設定一覧を検証してから、編集中の配置を置き換える。
	return ReplaceObjects(std::move(objects), error, true);
}

bool WorldObjects::ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error, bool areModelFilesRequired) {
	// 設定一覧を検証してから、編集中の配置を置き換える。
	error.clear();

	// 個数上限を超えた一覧は、配置へ反映しない。
	if (objects.size() > kMaxObjects) {
		error = "Too many scene objects.";

		return false;
	}

	// IDの重複を検出しながら、種類別の個数も数える。
	std::unordered_set<WorldObjectId> ids;

	WorldObjectId nextId = nextId_;

	size_t lightCount = 0;
	size_t spotCount = 0;
	size_t directionalCount = 0;
	size_t particleCount = 0;
	size_t mainCameraCount = 0;
	size_t colliderCount = 0;

	for (const WorldObjectData& object : objects) {
		if (object.collider.isPresent) {
			if (++colliderCount > kMaxWorldColliders) {
				error = "Scene supports up to 256 colliders.";

				return false;
			}
		}

		if (!IsObjectValid(object, areModelFilesRequired)) {
			error = "Invalid object data or unsupported model: " + std::to_string(object.id) + " / " + object.model;

			return false;
		}

		// 同じIDの重複は選択や親子参照を壊すため拒否する。
		if (!ids.insert(object.id).second) {
			error = "Duplicate object ID.";

			return false;
		}

		if (object.type == WorldObjectType::PointLight) {
			++lightCount;
		}

		if (object.type == WorldObjectType::SpotLight) {
			++spotCount;
		}

		if (object.type == WorldObjectType::DirectionalLight) {
			++directionalCount;
		}

		if (object.type == WorldObjectType::Particle) {
			// Scene上限はComponent数ではなく、GPU状態を持つEmitterノード数で数える。
			if (!IsParticleEffectDocumentValid(object.particleEffect)) {
				error = "Invalid particle effect document.";
				return false;
			}
			for (const ParticleEffectNode& node : object.particleEffect.nodes) {
				if (node.type == ParticleNodeType::Emitter) {
					++particleCount;
				}
			}
		}

		if (object.isMainCamera) {
			if (++mainCameraCount > 1) {
				error = "Only one main camera is allowed.";

				return false;
			}
		}

		if (lightCount > kMaxPointLights || spotCount > kMaxSpotLights || directionalCount > kMaxDirectionalLights) {
			error = "Scene light capacity exceeded (Point:9 / Spot:8 / Directional:4).";

			return false;
		}

		if (particleCount > 32) {
			error = "Scene supports up to 32 particle effect emitters.";

			return false;
		}

		if (object.id >= nextId) {
			nextId = object.id + 1;
		}
	}

	// 検証用のID検索表を作る。
	std::unordered_map<WorldObjectId, size_t> indices;

	for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
		indices[objects[objectIndex].id] = objectIndex;
	}

	// 親の存在・循環・深さを、置換前に検証する。
	if (!ValidateHierarchy(objects, indices, error)) {
		return false;
	}

	// 全件の検証に成功したので、一覧と検索表をまとめて切り替える。
	objects_ = std::move(objects);

	objectIndices_ = std::move(indices);

	nextId_ = nextId;

	return true;
}

//=============================================================================================//
// Getter / 追加可否・階層操作

//=============================================================================================//
// Getter / 検索・追加可否

// 描画リソースの上限は無効状態も含めて数え、Activeを切り替えても破綻させない。
bool WorldObjects::CanCreateType(WorldObjectType type) const {
	// 種類ごとの個数制限を確認し、追加できるか判定する。
	size_t count = 0;

	for (const WorldObjectData& object : objects_) {
		if (object.type == type) {
			++count;
		}
	}

	if (LightTypes::IsLight(type)) {
		return count < LightTypes::GetLimit(type);
	}

	if (type == WorldObjectType::Particle) {
		return count < 32;
	}

	return true;
}


//=============================================================================================//
// 階層操作

bool WorldObjects::IsDescendantOf(WorldObjectId objectId, WorldObjectId ancestorId) const {
	// 親を順にたどり、指定した祖先に含まれるか確認する。
	for (size_t depth = 0; depth < kMaxHierarchyDepth; ++depth) {
		if (objectId == kInvalidWorldObjectId) {
			return false;
		}

		if (objectId == ancestorId) {
			return true;
		}

		const size_t objectIndex = FindObjectIndex(objectId);

		if (objectIndex == objects_.size()) {
			return false;
		}

		objectId = objects_[objectIndex].parentId;
	}

	return false;
}

void WorldObjects::RebuildObjectIndices() {
	// 現在の一覧から、IDと添字の対応を作り直す。
	objectIndices_.clear();

	for (size_t objectIndex = 0; objectIndex < objects_.size(); ++objectIndex) {
		objectIndices_[objects_[objectIndex].id] = objectIndex;
	}
}
