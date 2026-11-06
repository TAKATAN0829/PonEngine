#include "objects/WorldObjects.h"
#include "objects/WorldAssetReferences.h"
#include "settings/LightSettings.h"

#include <array>
#include <filesystem>
#include <cmath>
#include <limits>
#include <unordered_set>
#include <algorithm>
#include <cctype>
#include "MathUtility.h"

//=============================================================================================//
// 実装内補助定義

namespace {
	constexpr size_t kMaxHierarchyDepth = 64;

	Vector3 CrossProduct(const Vector3& first, const Vector3& second) {
		// 成分ごとの積と差から外積を求める。
		return {first.y * second.z - first.z * second.y,
			first.z * second.x - first.x * second.z, first.x * second.y - first.y * second.x};
	}

	// 逆行列を作る前に、ゼロ拡縮などで潰れた親の変換を拒否する。
	bool IsInvertibleAffine(const Matrix4x4& matrix) {
		// 平行移動以外の成分から、逆変換できる行列か確認する。
		const Vector3 firstRow = {matrix.m[0][0], matrix.m[0][1], matrix.m[0][2]};
		const Vector3 secondRow = {matrix.m[1][0], matrix.m[1][1], matrix.m[1][2]};
		const Vector3 thirdRow = {matrix.m[2][0], matrix.m[2][1], matrix.m[2][2]};

		return std::abs(MathUtility::Dot(CrossProduct(firstRow, secondRow), thirdRow)) > 0.00000001f;
	}

	// 行ベクトルのS*Rx*Ry*Rz*TをローカルTRSへ戻す。せん断を近似して見た目を変えない。
	bool DecomposeAffine(const Matrix4x4& matrix, TransformData& result) {
		// 行列から位置・回転・拡縮を取り出し、表現できる変換か検証する。
		Vector3 basis[3] = {{matrix.m[0][0],matrix.m[0][1],matrix.m[0][2]},
			{matrix.m[1][0],matrix.m[1][1],matrix.m[1][2]}, {matrix.m[2][0],matrix.m[2][1],matrix.m[2][2]}};

		result.scale = {MathUtility::Length(basis[0]), MathUtility::Length(basis[1]), MathUtility::Length(basis[2])};

		if (result.scale.x < 0.000001f || result.scale.y < 0.000001f || result.scale.z < 0.000001f) {
			return false;
		}

		// 軸の向きが反転している場合は、その符号を拡縮へ移して回転から分離する。
		if (MathUtility::Dot(CrossProduct(basis[0], basis[1]), basis[2]) < 0) {
			result.scale.x = -result.scale.x;
		}

		basis[0] = MathUtility::Multiply(1.0f / result.scale.x, basis[0]);

		basis[1] = MathUtility::Multiply(1.0f / result.scale.y, basis[1]);

		basis[2] = MathUtility::Multiply(1.0f / result.scale.z, basis[2]);

		result.rotate.y = std::asin((std::clamp)(-basis[0].z, -1.0f, 1.0f));

		// Y軸が約90度のときはX軸とZ軸の回転を個別に求められないため、別の式で復元する。
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

//=============================================================================================//
// Getter / オブジェクト一覧

size_t WorldObjects::GetEditableObjectCount() const {
	// 編集中のオブジェクト数を取得する。
	return objects_.size();
}

// シーンが所有する値への借用参照を返す。追加・削除後は取得し直すこと。
EditableObjectView WorldObjects::GetEditableObject(size_t index) {
	// 一覧の範囲を確認し、設定を書き換えるための参照を返す。
	if (index >= objects_.size()) {
		return {kInvalidWorldObjectId, ""};
	}

	WorldObjectData& object = objects_[index];

	return {object.id, object.name.c_str(), &object.transform, &object.color, &object.isLightingEnabled,
		&object.shininess, &object.model, &object.isRootNodeMatrixUsed, &object.name,
		&object.isActive, &object.hasMeshRenderer, object.type, &object.lightIntensity,
		&object.lightRadius, &object.lightDecay, &object.particle, object.parentId, &object.camera, object.isMainCamera,
		&object.materials, &object.animation, &object.body, &object.collider, &object.motion, &object.light};
}

// 読取専用の比較では文字列を含むシーン全体を複製しない。
const WorldObjectData* WorldObjects::GetObjectData(size_t index) const {
	// 添字の範囲を確認して、保存対象の設定を参照する。
	if (index >= objects_.size()) {
		return nullptr;
	}

	return &objects_[index];
}

const WorldObjectData* WorldObjects::GetObjectDataById(WorldObjectId identifier) const {
	// IDに対応するオブジェクトの設定を検索する。
	return GetObjectData(FindObjectIndex(identifier));
}

WorldObjectId WorldObjects::GetNextObjectId() const {
	// 次の作成に使用する未使用IDを取得する。
	return nextId_;
}

const std::vector<WorldObjectData>& WorldObjects::GetObjects() const {
	// 保存対象のオブジェクト一覧を取得する。
	return objects_;
}

std::vector<WorldObjectData> WorldObjects::CaptureObjects() const {
	// 保存や履歴に使うため、設定一覧を値として返す。
	return objects_;
}

bool WorldObjects::HasObjectCollectionSupport() const {
	// 設定一覧の保存と置換に対応する実装か返す。
	return true;
}

//=============================================================================================//
// Getter・Setter / 親子変換

bool WorldObjects::GetObjectWorldMatrix(WorldObjectId id, Matrix4x4& world) const {
	// 親のTransformを順に合成し、ワールド行列を求める。
	world = MathUtility::MakeIdentity4x4();

	for (size_t depth = 0; depth < kMaxHierarchyDepth; ++depth) {
		const size_t objectIndex = FindObjectIndex(id);

		if (objectIndex == objects_.size()) {
			return false;
		}

		const WorldObjectData& object = objects_[objectIndex];

		world = MathUtility::Multiply(world, MathUtility::MakeAffineMatrix(object.transform.scale, object.transform.rotate, object.transform.translate));

		id = object.parentId;

		if (id == kInvalidWorldObjectId) {
			return true;
		}
	}

	return false;
}

bool WorldObjects::SetObjectWorldTransform(WorldObjectId id, const Matrix4x4& world) {
	// 親の変換を除き、保存できるローカルTransformへ分解する。
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

bool WorldObjects::SetObjectWorldPosition(WorldObjectId id, const Vector3& position) {
	// 親の逆行列を使い、ワールド位置をローカル位置へ変換する。
	const size_t objectIndex = FindObjectIndex(id);

	if (objectIndex == objects_.size()) {
		return false;
	}

	Vector3 localPosition = position;

	if (objects_[objectIndex].parentId != kInvalidWorldObjectId) {
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

bool WorldObjects::IsObjectActive(WorldObjectId id) const {
	// 自身と親の有効状態をたどり、実際に表示・更新する対象か判定する。
	for (size_t depth = 0; depth < kMaxHierarchyDepth; ++depth) {
		const size_t objectIndex = FindObjectIndex(id);

		if (objectIndex == objects_.size()) {
			return false;
		}

		if (!objects_[objectIndex].isActive) {
			return false;
		}

		id = objects_[objectIndex].parentId;

		if (id == kInvalidWorldObjectId) {
			return true;
		}
	}

	return false;
}

bool WorldObjects::SetObjectParent(WorldObjectId childId, WorldObjectId parentId, std::string& error) {
	// 循環と変換を検証し、見た目を保って親子関係を変更する。
	error.clear();

	const size_t childIndex = FindObjectIndex(childId);

	if (childIndex == objects_.size()) {
		error = "Child object does not exist.";

		return false;
	}

	if (objects_[childIndex].parentId == parentId) {
		return true;
	}

	// 新しい親の存在と、親子の循環を確認する。
	if (parentId != kInvalidWorldObjectId) {
		if (FindObjectIndex(parentId) == objects_.size()) {
			error = "Parent object does not exist.";

			return false;
		}

		// 自分の子孫を親にする循環を拒否する。
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

	if (parentId != kInvalidWorldObjectId) {
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

	std::vector<WorldObjectData> replaced = objects_;

	replaced[childIndex].parentId = parentId;

	replaced[childIndex].transform = localTransform;

	return ReplaceObjects(std::move(replaced), error);
}

//=============================================================================================//
// Getter・Setter / メインカメラ

// 0を指定するとメイン指定を解除する。失敗時は指定を変更しない。
bool WorldObjects::SetMainCamera(WorldObjectId id) {
	// 指定IDが有効なCameraか確認し、メイン指定を切り替える。
	if (id != kInvalidWorldObjectId) {
		const size_t objectIndex = FindObjectIndex(id);

		if (objectIndex == objects_.size()) {
			return false;
		}

		if (objects_[objectIndex].type != WorldObjectType::Camera) {
			return false;
		}
	}

	for (WorldObjectData& object : objects_) {
		object.isMainCamera = object.id == id;
	}

	return true;
}

WorldObjectId WorldObjects::GetMainCameraId() const {
	// メインに指定されているカメラのIDを取得する。
	for (const WorldObjectData& object : objects_) {
		if (object.isMainCamera) {
			return object.id;
		}
	}

	return kInvalidWorldObjectId;
}

// カメラの向きは親から継承し、拡縮やせん断は視野へ持ち込まない。
bool WorldObjects::GetGameCameraView(float aspectRatio, CameraView& view) const {
	// メインカメラの視野情報を取得する。
	return GetCameraView(GetMainCameraId(), aspectRatio, view);
}

bool WorldObjects::GetCameraView(WorldObjectId cameraId, float aspectRatio, CameraView& view) const {
	// 指定カメラの視野情報を取得する。
	if (!std::isfinite(aspectRatio) || aspectRatio <= 0) {
		return false;
	}

	const size_t cameraIndex = FindObjectIndex(cameraId);

	if (cameraIndex >= objects_.size()) {
		return false;
	}

	if (objects_[cameraIndex].type != WorldObjectType::Camera) {
		return false;
	}

	if (!IsObjectActive(cameraId)) {
		return false;
	}

	if (!IsObjectValid(objects_[FindObjectIndex(cameraId)])) {
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

	const CameraSettings& lens = objects_[FindObjectIndex(cameraId)].camera;

	const Matrix4x4 projection = MathUtility::MakePerspectiveFovMatrix(lens.fieldOfView, aspectRatio, lens.nearClip, lens.farClip);

	view.viewProjection = MathUtility::Multiply(MathUtility::Inverse(view.cameraWorld), projection);

	return true;
}

//=============================================================================================//
// Asset参照の更新

void WorldObjects::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	WorldAssetReferences::Remap(objects_, change);
}

//=============================================================================================//
// 追加・複製・削除

// ドロップ配置は一括検証してから追加し、失敗した途中のオブジェクトを残さない。
WorldObjectId WorldObjects::PlaceModel(const std::string& model, WorldObjectId parentId, const Vector3& position, std::string& error) {
	// モデルと親子変換を検証し、配置できる場合だけ一覧へ追加する。
	error.clear();

	if (!IsSupportedModel(model)) {
		error = "このモデルはまだ読み込まれていません。";

		return kInvalidWorldObjectId;
	}

	if (objects_.size() >= kMaxObjects) {
		error = "オブジェクト数の上限です。";

		return kInvalidWorldObjectId;
	}

	if (nextId_ == (std::numeric_limits<WorldObjectId>::max)()) {
		error = "IDの上限です。";

		return kInvalidWorldObjectId;
	}

	if (!PrepareModel(model,error)) {
		return kInvalidWorldObjectId;
	}

	WorldObjectData object;

	object.id = nextId_;

	object.name = model + " #" + std::to_string(object.id);

	object.model = model;

	object.parentId = parentId;

	object.transform.translate = position;

	if (parentId != kInvalidWorldObjectId) {
		Matrix4x4 parentWorld{};

		if (!GetObjectWorldMatrix(parentId, parentWorld)) {
			error = "親が見つかりません。";

			return kInvalidWorldObjectId;
		}

		if (!IsInvertibleAffine(parentWorld)) {
			error = "親の拡縮が0に近いため配置できません。";

			return kInvalidWorldObjectId;
		}

		object.transform.translate = MathUtility::Transform(position, MathUtility::Inverse(parentWorld));
	}

	std::vector<WorldObjectData> replaced = objects_;

	replaced.push_back(object);

	if (!ReplaceObjects(std::move(replaced), error)) {
		return kInvalidWorldObjectId;
	}

	return object.id;
}

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
			++particleCount;
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
			error = "Scene supports up to 32 particle emitters.";

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
// 保存データ検証

// 対応拡張子とresources相対パスの形だけを検証する。
bool WorldObjects::IsSupportedModelPath(const std::string& model) {
	// モデルの拡張子とパスの構成を確認する。
	if (model == "plane.gltf" || model == "axis.obj") {
		return true;
	}

	if (model.empty() || model.size() > 255 || model.find('\0') != std::string::npos) {
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

	return true;
}

bool WorldObjects::IsSupportedModel(const std::string& model) {
	// 組込モデルまたは利用できるモデルファイルか確認する。
	if (!IsSupportedModelPath(model)) {
		return false;
	}

	if (model == "plane.gltf" || model == "axis.obj") {
		return true;
	}

	const std::filesystem::path path(std::u8string(model.begin(), model.end()));

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
bool WorldObjects::IsObjectValid(const WorldObjectData& object, bool areModelFilesRequired) {
	if (!AreLightSettingsValid(object.light)) {
		return false;
	}

	if (!AreBodySettingsValid(object.body)) {
		return false;
	}

	if (!AreColliderSettingsValid(object.collider)) {
		return false;
	}

	if (!AreMotionSettingsValid(object.motion)) {
		return false;
	}

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

	if (object.id == kInvalidWorldObjectId || object.id == (std::numeric_limits<WorldObjectId>::max)()) {
		return false;
	}

	if (object.name.empty() || object.name.size() > 256 || object.name.find('\0') != std::string::npos) {
		return false;
	}

	if (!IsSupportedModelPath(object.model)) {
		return false;
	}

	if (areModelFilesRequired) {
		if (!IsSupportedModel(object.model)) {
			return false;
		}
	}

	if (object.materials.size() > 256) {
		return false;
	}

	std::unordered_set<uint32_t> materialSlots;

	for (const MaterialSettings& material : object.materials) {
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

	if (object.type < WorldObjectType::Model || object.type > WorldObjectType::DirectionalLight) {
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

	if (object.type == WorldObjectType::Particle) {
		if (!AreParticleSettingsValid(object.particle)) {
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

	if (object.isMainCamera) {
		if (object.type != WorldObjectType::Camera) {
			return false;
		}
	}

	return true;
}

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

// ID検索はハッシュ表を使い、親をたどるたびに一覧を走査しない。
size_t WorldObjects::FindObjectIndex(WorldObjectId objectId) const {
	// ID検索用の一覧から、対象の位置を求める。
	const std::unordered_map<WorldObjectId, size_t>::const_iterator found = objectIndices_.find(objectId);

	if (found == objectIndices_.end()) {
		return objects_.size();
	}

	return found->second;
}

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

// 親参照の検証は値の検証と分け、順不同の保存データも正しく扱う。
bool WorldObjects::ValidateHierarchy(const std::vector<WorldObjectData>& objects,
	const std::unordered_map<WorldObjectId, size_t>& indices, std::string& error) {
	for (const WorldObjectData& object : objects) {
		WorldObjectId parentId = object.parentId;

		size_t depth = 1;

		while (parentId != kInvalidWorldObjectId) {
			if (parentId == object.id) {
				error = "Parenting would create a cycle.";

				return false;
			}

			if (++depth > kMaxHierarchyDepth) {
				error = "Hierarchy exceeds 64 levels or contains a cycle.";

				return false;
			}

			const std::unordered_map<WorldObjectId, size_t>::const_iterator found = indices.find(parentId);

			if (found == indices.end()) {
				error = "Parent object does not exist.";

				return false;
			}

			// Dynamicの子はColliderだけで構成し、独立したRigidbodyの二重移動を防ぐ。
			if (object.body.isPresent && object.body.isEnabled) {
				const RigidBodySettings& parentBody = objects[found->second].body;

				if (parentBody.isPresent && parentBody.isEnabled && parentBody.mode == BodyMode::Dynamic) {
					error = "Rigidbody cannot have a Dynamic Rigidbody ancestor. Use child Colliders without another Rigidbody.";

					return false;
				}

				if (object.body.mode == BodyMode::Dynamic) {
					if (parentBody.isPresent && parentBody.isEnabled && parentBody.mode == BodyMode::Kinematic) {
						error = "Dynamic Rigidbody requires a stationary parent. Put the Dynamic body outside the Kinematic hierarchy.";

						return false;
					}
				}
			}

			parentId = objects[found->second].parentId;
		}
	}

	return true;
}
