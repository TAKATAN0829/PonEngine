#include "SceneObjects.h"
#include "PrefabFile.h"

#include <array>
#include <filesystem>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include "MathUtility.h"

namespace {
	constexpr size_t kMaxHierarchyDepth = 64;

	Vector3 CrossProduct(const Vector3& first, const Vector3& second) {
		return {first.y * second.z - first.z * second.y,
			first.z * second.x - first.x * second.z, first.x * second.y - first.y * second.x};
	}

	// 逆行列を作る前に、ゼロ拡縮などで潰れた親の変換を拒否する。
	bool IsInvertibleAffine(const Matrix4x4& matrix) {
		const Vector3 firstRow = {matrix.m[0][0], matrix.m[0][1], matrix.m[0][2]};
		const Vector3 secondRow = {matrix.m[1][0], matrix.m[1][1], matrix.m[1][2]};
		const Vector3 thirdRow = {matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]};
		return std::abs(MathUtility::Dot(CrossProduct(firstRow, secondRow), thirdRow)) > 0.00000001f;
	}

	// 行ベクトルのS*Rx*Ry*Rz*TをローカルTRSへ戻す。せん断を近似して見た目を変えない。
	bool DecomposeAffine(const Matrix4x4& matrix, TransformData& result) {
		Vector3 basis[3] = {{matrix.m[0][0],matrix.m[0][1],matrix.m[0][2]},
			{matrix.m[1][0],matrix.m[1][1],matrix.m[1][2]}, {matrix.m[2][0],matrix.m[2][1],matrix.m[2][2]}};
		result.scale = {MathUtility::Length(basis[0]), MathUtility::Length(basis[1]), MathUtility::Length(basis[2])};
		if (result.scale.x < 0.000001f || result.scale.y < 0.000001f || result.scale.z < 0.000001f) {
			return false;
		}
		if (MathUtility::Dot(CrossProduct(basis[0], basis[1]), basis[2]) < 0) {
			result.scale.x = -result.scale.x;
		}
		basis[0] = MathUtility::Multiply(1.0f / result.scale.x, basis[0]);
		basis[1] = MathUtility::Multiply(1.0f / result.scale.y, basis[1]);
		basis[2] = MathUtility::Multiply(1.0f / result.scale.z, basis[2]);
		result.rotate.y = std::asin((std::clamp)(-basis[0].z, -1.0f, 1.0f));
		if (std::abs(std::cos(result.rotate.y)) > 0.0001f) {
			result.rotate.x = std::atan2(basis[1].z, basis[2].z);
			result.rotate.z = std::atan2(basis[0].y, basis[0].x);
		} else {
			result.rotate.x = std::atan2(-basis[2].y, basis[1].y);
			result.rotate.z = 0;
		}
		result.translate = {matrix.m[3][0], matrix.m[3][1], matrix.m[3][2]};
		const Matrix4x4 reconstructed = MathUtility::MakeAffineMatrix(result.scale, result.rotate, result.translate);
		for (size_t rowIndex = 0; rowIndex < 4; ++rowIndex) {
			for (size_t columnIndex = 0; columnIndex < 4; ++columnIndex) {
				if (!std::isfinite(matrix.m[rowIndex][columnIndex])) {
					return false;
				}
				if (std::abs(matrix.m[rowIndex][columnIndex] - reconstructed.m[rowIndex][columnIndex]) >
					0.0001f * (1 + std::abs(matrix.m[rowIndex][columnIndex]))) {
					return false;
				}
			}
		}
		return true;
	}
}

SceneObjectId SceneObjects::PlacePrefab(const std::string& prefab, SceneObjectId parentId, const Vector3& position, std::string& error) {
	const std::filesystem::path relative(std::u8string(prefab.begin(), prefab.end()));
	if (relative.is_absolute() || relative.has_root_name() || relative.extension() != ".ponprefab") {
		error = "Invalid prefab path.";
		return 0;
	}
	for (const std::filesystem::path& part : relative) {
		if (part == "..") {
			error = "Prefab must be inside resources.";
			return 0;
		}
	}
	std::error_code fileError;
	const std::filesystem::path root = std::filesystem::weakly_canonical("resources", fileError);
	const std::filesystem::path resolved = std::filesystem::weakly_canonical(root / relative, fileError);
	if (fileError || resolved.lexically_relative(root).empty()) {
		error = "Cannot resolve prefab path.";
		return 0;
	}
	if (*resolved.lexically_relative(root).begin() == "..") {
		error = "Prefab must be inside resources.";
		return 0;
	}
	return PrefabFile::Instantiate(*this, resolved, parentId, position, error);
}

// 対応形式とresources内の実ファイルを検証し、外部への参照を防ぐ。
bool SceneObjects::IsSupportedModel(const std::string& model) {
	if (model == "plane.gltf" || model == "axis.obj") {
		return true;
	}
	if (model.empty() || model.find('\0') != std::string::npos) {
		return false;
	}
	const std::filesystem::path path = std::filesystem::path(std::u8string(model.begin(),model.end()));
	if (path.is_absolute() || path.has_root_name()) {
		return false;
	}
	for (const std::filesystem::path& part : path) {
		if (part == "..") {
			return false;
		}
	}
	std::string extension = path.extension().string();
	for (char& character : extension) {
		character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
	}
	if (extension != ".obj" && extension != ".gltf" && extension != ".glb" && extension != ".fbx") {
		return false;
	}
	std::error_code error;
	std::filesystem::path root = "resources";
	if (!std::filesystem::is_directory(root,error)) {
		root = "project/CG2/resources";
	}
	root = std::filesystem::weakly_canonical(root,error);
	const std::filesystem::path resolved = std::filesystem::weakly_canonical(root / path,error);
	if (error) {
		return false;
	}
	const std::filesystem::path relative = resolved.lexically_relative(root);
	if (relative.empty()) {
		return false;
	}
	if (*relative.begin() == "..") {
		return false;
	}
	return std::filesystem::is_regular_file(resolved,error);
}

// 保存や復元に使えない数値・名前・ID・モデル指定を検出する。
bool SceneObjects::Validate(const SceneObjectData& object) {
	if (!object.prefabSource.empty()) {
		if (object.prefabSource.find('\0') != std::string::npos) {
			return false;
		}
		const std::filesystem::path source(std::u8string(object.prefabSource.begin(), object.prefabSource.end()));
		if (object.prefabSource.size() > 512 || source.is_absolute() || source.has_root_name() || source.extension() != ".ponprefab") {
			return false;
		}
		for (const std::filesystem::path& part : source) {
			if (part == "..") {
				return false;
			}
		}
	}
	const TransformData& transform = object.transform;
	const Vector4& color = object.color;
	const std::array<float, 14> values = {transform.scale.x, transform.scale.y, transform.scale.z,
		transform.rotate.x, transform.rotate.y, transform.rotate.z,
		transform.translate.x, transform.translate.y, transform.translate.z,
		color.x, color.y, color.z, color.w, object.shininess};
	for (float value : values) {
		if (!std::isfinite(value)) {
			return false;
		}
	}
	if (object.id == kInvalidSceneObjectId || object.id == (std::numeric_limits<SceneObjectId>::max)()) {
		return false;
	}
	if (object.name.empty() || object.name.size() > 256 || object.name.find('\0') != std::string::npos) {
		return false;
	}
	if (!IsSupportedModel(object.model)) {
		return false;
	}
	if (object.materials.size() > 256) {
		return false;
	}
	std::unordered_set<uint32_t> materialSlots;
	for (const SceneMaterialSettings& material : object.materials) {
		if (material.slot >= 256 || !materialSlots.insert(material.slot).second) {
			return false;
		}
		if (!std::isfinite(material.shininess) || material.shininess < 1 || material.shininess > 500) {
			return false;
		}
		if (!std::isfinite(material.color.x) || !std::isfinite(material.color.y) || !std::isfinite(material.color.z) || !std::isfinite(material.color.w)) {
			return false;
		}
		if (!material.texture.empty()) {
			if (material.texture.size() > 255 || material.texture.find('\0') != std::string::npos) {
				return false;
			}
			const std::filesystem::path texturePath(std::u8string(material.texture.begin(), material.texture.end()));
			if (texturePath.is_absolute() || texturePath.has_root_name()) {
				return false;
			}
			for (const std::filesystem::path& part : texturePath) {
				if (part == "..") {
					return false;
				}
			}
		}
	}
	if (object.animation.clip < 0 || object.animation.clip > 4095) {
		return false;
	}
	if (!std::isfinite(object.animation.speed) || object.animation.speed < 0 || object.animation.speed > 10) {
		return false;
	}
	if (!std::isfinite(object.animation.time) || object.animation.time < 0 || object.animation.time > 86400) {
		return false;
	}
	if (object.shininess < 1 || object.shininess > 500) {
		return false;
	}
	if (object.type < SceneObjectType::Model || object.type > SceneObjectType::Camera) {
		return false;
	}
	// 同じ光源でも、強度・範囲・減衰は別の値として検証する。
	if (!std::isfinite(object.lightIntensity) || object.lightIntensity < 0 || object.lightIntensity > 100) {
		return false;
	}
	if (!std::isfinite(object.lightRadius) || object.lightRadius < 0.01f || object.lightRadius > 1000) {
		return false;
	}
	if (!std::isfinite(object.lightDecay) || object.lightDecay < 0.01f || object.lightDecay > 100) {
		return false;
	}
	if (object.type == SceneObjectType::Particle) {
		if (!ValidateParticleSettings(object.particle)) {
			return false;
		}
	}
	if (!std::isfinite(object.camera.fieldOfView) || object.camera.fieldOfView < 0.0174532925f || object.camera.fieldOfView > 3.12413936f) {
		return false;
	}
	if (!std::isfinite(object.camera.nearClip) || object.camera.nearClip < 0.001f) {
		return false;
	}
	if (!std::isfinite(object.camera.farClip) || object.camera.farClip <= object.camera.nearClip || object.camera.farClip > 1000000) {
		return false;
	}
	if (object.mainCamera) {
		if (object.type != SceneObjectType::Camera) {
			return false;
		}
	}
	return true;
}

// シーンが所有する値への借用参照を返す。追加・削除後は取得し直すこと。
EditableObjectView SceneObjects::GetEditableObject(size_t index) {
	if (index >= objects_.size()) {
		return {};
	}
	SceneObjectData& object = objects_[index];
	return {object.id, object.name.c_str(), &object.transform, &object.color, &object.enableLighting,
		&object.shininess, &object.model, &object.useRootNodeMatrix, &object.name,
		&object.active, &object.hasMeshRenderer, object.type, &object.lightIntensity,
		&object.lightRadius, &object.lightDecay, &object.particle, object.parentId, &object.camera, object.mainCamera,
		&object.materials, &object.animation};
}

// 読取専用の比較では文字列を含むシーン全体を複製しない。
const SceneObjectData* SceneObjects::GetObjectData(size_t index) const {
	if (index >= objects_.size()) {
		return nullptr;
	}
	return &objects_[index];
}

const SceneObjectData* SceneObjects::GetObjectDataById(SceneObjectId identifier) const {
	return GetObjectData(FindObjectIndex(identifier));
}

// GPUリソースを作らず、種類に応じた編集用データだけを追加する。
SceneObjectId SceneObjects::CreateObject(SceneObjectType type) {
	if (type < SceneObjectType::Model || type > SceneObjectType::Camera) {
		return kInvalidSceneObjectId;
	}
	if (!CanCreateType(type)) {
		return kInvalidSceneObjectId;
	}
	const SceneObjectId createdId = AddObject("plane.gltf");
	if (createdId == kInvalidSceneObjectId) {
		return createdId;
	}
	SceneObjectData& object = objects_.back();
	object.type = type;
	object.hasMeshRenderer = type == SceneObjectType::Model;
	std::string typeName = "Model";
	if (type == SceneObjectType::Empty) {
		typeName = "Empty";
	}
	if (type == SceneObjectType::PointLight) {
		typeName = "Point Light";
		object.transform.translate.y = 2;
	}
	if (type == SceneObjectType::Particle) {
		typeName = "Particle";
	}
	if (type == SceneObjectType::Camera) {
		typeName = "Camera";
		object.transform.translate = {0,0,-30};
		object.mainCamera = GetMainCameraId() == kInvalidSceneObjectId;
	}
	object.name = typeName + " #" + std::to_string(createdId);
	return createdId;
}

// 新しいIDと既定の設定を持つモデルを追加する。削除済みIDは再利用しない。
SceneObjectId SceneObjects::AddObject(const std::string& model) {
	if (!IsSupportedModel(model)) {
		return kInvalidSceneObjectId;
	}
	if (objects_.size() >= kMaxObjects) {
		return kInvalidSceneObjectId;
	}
	if (nextId_ == (std::numeric_limits<SceneObjectId>::max)()) {
		return kInvalidSceneObjectId;
	}
	std::string importError;
	if (!PrepareModel(model, importError)) {
		return kInvalidSceneObjectId;
	}
	SceneObjectData object;
	object.id = nextId_;
	object.model = model;
	object.name = model + " #" + std::to_string(object.id);
	objects_.push_back(std::move(object));
	objectIndices_[nextId_] = objects_.size() - 1;
	return nextId_++;
}

// 設定を値として複製し、新しいIDを付けて元のモデルから独立させる。
SceneObjectId SceneObjects::DuplicateObject(SceneObjectId id) {
	if (FindObjectIndex(id) == objects_.size()) {
		return kInvalidSceneObjectId;
	}
	std::vector<SceneObjectData> copies;
	std::unordered_map<SceneObjectId, SceneObjectId> newIds;
	SceneObjectId nextId = nextId_;
	for (const SceneObjectData& object : objects_) {
		if (!IsDescendantOf(object.id, id)) {
			continue;
		}
		if (nextId == (std::numeric_limits<SceneObjectId>::max)()) {
			return kInvalidSceneObjectId;
		}
		copies.push_back(object);
		newIds[object.id] = nextId++;
	}
	std::vector<SceneObjectData> replaced = objects_;
	for (SceneObjectData& copy : copies) {
		copy.id = newIds.at(copy.id);
		copy.mainCamera = false;
		// 複製でメイン指定を増やさない。
		if (copy.parentId != kInvalidSceneObjectId) {
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
		return kInvalidSceneObjectId;
	}
	return newIds.at(id);
}

// 指定IDと子孫をまとめて削除し、残るオブジェクトのIDは変更しない。
bool SceneObjects::RemoveObject(SceneObjectId id) {
	if (FindObjectIndex(id) == objects_.size()) {
		return false;
	}
	std::unordered_set<SceneObjectId> removedIds;
	for (const SceneObjectData& object : objects_) {
		if (IsDescendantOf(object.id, id)) {
			removedIds.insert(object.id);
		}
	}
	std::vector<SceneObjectData>::iterator objectIterator = objects_.begin();
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

// ID検索はハッシュ表を使い、親をたどるたびに一覧を走査しない。
size_t SceneObjects::FindObjectIndex(SceneObjectId objectId) const {
	const std::unordered_map<SceneObjectId, size_t>::const_iterator found = objectIndices_.find(objectId);
	if (found == objectIndices_.end()) {
		return objects_.size();
	}
	return found->second;
}

void SceneObjects::RebuildObjectIndices() {
	objectIndices_.clear();
	for (size_t objectIndex = 0; objectIndex < objects_.size(); ++objectIndex) {
		objectIndices_[objects_[objectIndex].id] = objectIndex;
	}
}

bool SceneObjects::IsDescendantOf(SceneObjectId objectId, SceneObjectId ancestorId) const {
	for (size_t depth = 0; depth < kMaxHierarchyDepth; ++depth) {
		if (objectId == kInvalidSceneObjectId) {
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

// 全件の検証が済んでから一覧を置き換え、不正データの部分反映を防ぐ。
bool SceneObjects::ReplaceObjects(std::vector<SceneObjectData> objects, std::string& error) {
	error.clear();
	if (objects.size() > kMaxObjects) {
		error = "Too many scene objects.";
		return false;
	}
	std::unordered_set<SceneObjectId> ids;
	SceneObjectId nextId = nextId_;
	size_t lightCount = 0;
	size_t particleCount = 0;
	size_t mainCameraCount = 0;
	for (const SceneObjectData& object : objects) {
		if (!Validate(object)) {
			error = "Invalid object data or unsupported model.";
			return false;
		}
		if (!ids.insert(object.id).second) {
			error = "Duplicate object ID.";
			return false;
		}
		if (object.type == SceneObjectType::PointLight) {
			++lightCount;
		}
		if (object.type == SceneObjectType::Particle) {
			++particleCount;
		}
		if (object.mainCamera) {
			if (++mainCameraCount > 1) {
				error = "Only one main camera is allowed.";
				return false;
			}
		}
		if (lightCount > 8 || particleCount > 32) {
			error = "Scene supports up to 8 point lights and 32 particle emitters.";
			return false;
		}
		if (object.id >= nextId) {
			nextId = object.id + 1;
		}
	}
	std::unordered_map<SceneObjectId, size_t> indices;
	for (size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
		indices[objects[objectIndex].id] = objectIndex;
	}
	if (!ValidateHierarchy(objects, indices, error)) {
		return false;
	}
	objects_ = std::move(objects);
	objectIndices_ = std::move(indices);
	nextId_ = nextId;
	return true;
}

// 描画リソースの上限は無効状態も含めて数え、Activeを切り替えても破綻させない。
bool SceneObjects::CanCreateType(SceneObjectType type) const {
	size_t count = 0;
	for (const SceneObjectData& object : objects_) {
		if (object.type == type) {
			++count;
		}
	}
	if (type == SceneObjectType::PointLight) {
		return count < 8;
	}
	if (type == SceneObjectType::Particle) {
		return count < 32;
	}
	return true;
}

// 親参照の検証は値の検証と分け、順不同の保存データも正しく扱う。
bool SceneObjects::ValidateHierarchy(const std::vector<SceneObjectData>& objects,
	const std::unordered_map<SceneObjectId, size_t>& indices, std::string& error) {
	for (const SceneObjectData& object : objects) {
		SceneObjectId parentId = object.parentId;
		size_t depth = 1;
		while (parentId != kInvalidSceneObjectId) {
			if (parentId == object.id) {
				error = "Parenting would create a cycle.";
				return false;
			}
			if (++depth > kMaxHierarchyDepth) {
				error = "Hierarchy exceeds 64 levels or contains a cycle.";
				return false;
			}
			const std::unordered_map<SceneObjectId, size_t>::const_iterator found = indices.find(parentId);
			if (found == indices.end()) {
				error = "Parent object does not exist.";
				return false;
			}
			parentId = objects[found->second].parentId;
		}
	}
	return true;
}

bool SceneObjects::GetObjectWorldMatrix(SceneObjectId id, Matrix4x4& world) const {
	world = MathUtility::MakeIdentity4x4();
	for (size_t depth = 0; depth < kMaxHierarchyDepth; ++depth) {
		const size_t objectIndex = FindObjectIndex(id);
		if (objectIndex == objects_.size()) {
			return false;
		}
		const SceneObjectData& object = objects_[objectIndex];
		world = MathUtility::Multiply(world, MathUtility::MakeAffineMatrix(object.transform.scale, object.transform.rotate, object.transform.translate));
		id = object.parentId;
		if (id == kInvalidSceneObjectId) {
			return true;
		}
	}
	return false;
}

bool SceneObjects::IsObjectActive(SceneObjectId id) const {
	for (size_t depth = 0; depth < kMaxHierarchyDepth; ++depth) {
		const size_t objectIndex = FindObjectIndex(id);
		if (objectIndex == objects_.size()) {
			return false;
		}
		if (!objects_[objectIndex].active) {
			return false;
		}
		id = objects_[objectIndex].parentId;
		if (id == kInvalidSceneObjectId) {
			return true;
		}
	}
	return false;
}

bool SceneObjects::SetObjectWorldPosition(SceneObjectId id, const Vector3& position) {
	const size_t objectIndex = FindObjectIndex(id);
	if (objectIndex == objects_.size()) {
		return false;
	}
	Vector3 localPosition = position;
	if (objects_[objectIndex].parentId != kInvalidSceneObjectId) {
		Matrix4x4 parentWorld{};
		if (!GetObjectWorldMatrix(objects_[objectIndex].parentId, parentWorld)) {
			return false;
		}
		if (!IsInvertibleAffine(parentWorld)) {
			return false;
		}
		localPosition = MathUtility::Transform(position, MathUtility::Inverse(parentWorld));
	}
	if (!std::isfinite(localPosition.x) || !std::isfinite(localPosition.y) || !std::isfinite(localPosition.z)) {
		return false;
	}
	objects_[objectIndex].transform.translate = localPosition;
	return true;
}

bool SceneObjects::SetObjectParent(SceneObjectId childId, SceneObjectId parentId, std::string& error) {
	error.clear();
	const size_t childIndex = FindObjectIndex(childId);
	if (childIndex == objects_.size()) {
		error = "Child object does not exist.";
		return false;
	}
	if (objects_[childIndex].parentId == parentId) {
		return true;
	}
	if (parentId != kInvalidSceneObjectId) {
		if (FindObjectIndex(parentId) == objects_.size()) {
			error = "Parent object does not exist.";
			return false;
		}
		if (IsDescendantOf(parentId, childId)) {
			error = "Parenting would create a cycle.";
			return false;
		}
	}
	Matrix4x4 localMatrix{};
	if (!GetObjectWorldMatrix(childId, localMatrix)) {
		error = "Cannot resolve child transform.";
		return false;
	}
	if (parentId != kInvalidSceneObjectId) {
		Matrix4x4 parentWorld{};
		if (!GetObjectWorldMatrix(parentId, parentWorld)) {
			error = "Cannot resolve parent transform.";
			return false;
		}
		if (!IsInvertibleAffine(parentWorld)) {
			error = "Parent scale is zero or too small.";
			return false;
		}
		localMatrix = MathUtility::Multiply(localMatrix, MathUtility::Inverse(parentWorld));
	}
	TransformData localTransform{};
	if (!DecomposeAffine(localMatrix, localTransform)) {
		error = "Cannot preserve world transform with this rotation and non-uniform scale.";
		return false;
	}
	std::vector<SceneObjectData> replaced = objects_;
	replaced[childIndex].parentId = parentId;
	replaced[childIndex].transform = localTransform;
	return ReplaceObjects(std::move(replaced), error);
}

// 0を指定するとメイン指定を解除する。失敗時は指定を変更しない。
bool SceneObjects::SetMainCamera(SceneObjectId id) {
	if (id != kInvalidSceneObjectId) {
		const size_t objectIndex = FindObjectIndex(id);
		if (objectIndex == objects_.size()) {
			return false;
		}
		if (objects_[objectIndex].type != SceneObjectType::Camera) {
			return false;
		}
	}
	for (SceneObjectData& object : objects_) {
		object.mainCamera = object.id == id;
	}
	return true;
}

SceneObjectId SceneObjects::GetMainCameraId() const {
	for (const SceneObjectData& object : objects_) {
		if (object.mainCamera) {
			return object.id;
		}
	}
	return kInvalidSceneObjectId;
}

// カメラの向きは親から継承し、拡縮やせん断は視野へ持ち込まない。
bool SceneObjects::GetGameCameraView(float aspectRatio, SceneCameraView& view) const {
	if (!std::isfinite(aspectRatio) || aspectRatio <= 0) {
		return false;
	}
	const SceneObjectId cameraId = GetMainCameraId();
	if (!IsObjectActive(cameraId)) {
		return false;
	}
	if (!Validate(objects_[FindObjectIndex(cameraId)])) {
		return false;
	}
	Matrix4x4 world{};
	if (!GetObjectWorldMatrix(cameraId, world)) {
		return false;
	}
	Vector3 forward = {world.m[2][0], world.m[2][1], world.m[2][2]};
	Vector3 up = {world.m[1][0], world.m[1][1], world.m[1][2]};
	if (MathUtility::Length(forward) < 0.000001f) {
		return false;
	}
	forward = MathUtility::Normalize(forward);
	Vector3 right = CrossProduct(up, forward);
	if (MathUtility::Length(right) < 0.000001f) {
		return false;
	}
	right = MathUtility::Normalize(right);
	up = CrossProduct(forward, right);
	view.cameraWorld = MathUtility::MakeIdentity4x4();
	view.cameraWorld.m[0][0] = right.x; view.cameraWorld.m[0][1] = right.y; view.cameraWorld.m[0][2] = right.z;
	view.cameraWorld.m[1][0] = up.x; view.cameraWorld.m[1][1] = up.y; view.cameraWorld.m[1][2] = up.z;
	view.cameraWorld.m[2][0] = forward.x; view.cameraWorld.m[2][1] = forward.y; view.cameraWorld.m[2][2] = forward.z;
	view.cameraWorld.m[3][0] = world.m[3][0]; view.cameraWorld.m[3][1] = world.m[3][1]; view.cameraWorld.m[3][2] = world.m[3][2];
	const SceneCameraSettings& lens = objects_[FindObjectIndex(cameraId)].camera;
	const Matrix4x4 projection = MathUtility::MakePerspectiveFovMatrix(lens.fieldOfView, aspectRatio, lens.nearClip, lens.farClip);
	view.viewProjection = MathUtility::Multiply(MathUtility::Inverse(view.cameraWorld), projection);
	return true;
}

// ドロップ配置は一括検証してから追加し、失敗した途中のオブジェクトを残さない。
SceneObjectId SceneObjects::PlaceModel(const std::string& model, SceneObjectId parentId, const Vector3& position, std::string& error) {
	error.clear();
	if (!IsSupportedModel(model)) {
		error = "このモデルはまだ読み込まれていません。";
		return kInvalidSceneObjectId;
	}
	if (objects_.size() >= kMaxObjects) {
		error = "オブジェクト数の上限です。";
		return kInvalidSceneObjectId;
	}
	if (nextId_ == (std::numeric_limits<SceneObjectId>::max)()) {
		error = "IDの上限です。";
		return kInvalidSceneObjectId;
	}
	if (!PrepareModel(model,error)) {
		return kInvalidSceneObjectId;
	}
	SceneObjectData object;
	object.id = nextId_;
	object.name = model + " #" + std::to_string(object.id);
	object.model = model;
	object.parentId = parentId;
	object.transform.translate = position;
	if (parentId != kInvalidSceneObjectId) {
		Matrix4x4 parentWorld{};
		if (!GetObjectWorldMatrix(parentId, parentWorld)) {
			error = "親が見つかりません。";
			return kInvalidSceneObjectId;
		}
		if (!IsInvertibleAffine(parentWorld)) {
			error = "親の拡縮が0に近いため配置できません。";
			return kInvalidSceneObjectId;
		}
		object.transform.translate = MathUtility::Transform(position, MathUtility::Inverse(parentWorld));
	}
	std::vector<SceneObjectData> replaced = objects_;
	replaced.push_back(object);
	if (!ReplaceObjects(std::move(replaced), error)) {
		return kInvalidSceneObjectId;
	}
	return object.id;
}

bool SceneObjects::SetObjectWorldTransform(SceneObjectId id, const Matrix4x4& world) {
	const size_t index = FindObjectIndex(id);
	if (index == objects_.size()) {
		return false;
	}
	Matrix4x4 local = world;
	if (objects_[index].parentId != 0) {
		Matrix4x4 parent{};
		if (!GetObjectWorldMatrix(objects_[index].parentId,parent)) {
			return false;
		}
		if (!IsInvertibleAffine(parent)) {
			return false;
		}
		local = MathUtility::Multiply(world,MathUtility::Inverse(parent));
	}
	TransformData transform{};
	if (!DecomposeAffine(local,transform)) {
		return false;
	}
	objects_[index].transform = transform;
	return true;
}
