#include "objects/WorldObjects.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include "MathUtility.h"

//=============================================================================================//
// 実装内補助定義

namespace {

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

	// 旧ParticleSettings参照と、複数Emitterを含むEffect文書参照を同じViewで渡す。
	return {object.id, object.name.c_str(), &object.transform, &object.color, &object.isLightingEnabled,
		&object.shininess, &object.model, &object.isRootNodeMatrixUsed, &object.name,
		&object.isActive, &object.hasMeshRenderer, object.type, &object.lightIntensity,
		&object.lightRadius, &object.lightDecay, &object.particle, &object.particleEffect, object.parentId, &object.camera, object.isMainCamera,
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
	const size_t objectIndex = FindObjectIndex(identifier);

	if (objectIndex >= objects_.size()) {
		return nullptr;
	}

	return &objects_[objectIndex];
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

//=============================================================================================//
// Getter / 検索

// ID検索はハッシュ表を使い、親をたどるたびに一覧を走査しない。
size_t WorldObjects::FindObjectIndex(WorldObjectId objectId) const {
	// ID検索用の一覧から、対象の位置を求める。
	const std::unordered_map<WorldObjectId, size_t>::const_iterator found = objectIndices_.find(objectId);

	if (found == objectIndices_.end()) {
		return objects_.size();
	}

	return found->second;
}
