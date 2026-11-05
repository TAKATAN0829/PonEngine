#include "IWorldObjects.h"

//=============================================================================================//
// Getter / オブジェクト一覧・ID

// 履歴比較用の読み取り専用参照。一覧変更後には取得し直す。
// 公開できないシーンはnullptrを返し、CaptureObjectsでの比較へ切り替える。
const WorldObjectData* IWorldObjects::GetObjectData(size_t index) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	(void)index;

	return nullptr;
}

// ID検索を公開し、選択の検証で毎フレーム一覧のコピーを作らない。
const WorldObjectData* IWorldObjects::GetObjectDataById(WorldObjectId identifier) const {
	// IDに対応するオブジェクトの設定を検索する。
	for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
		const WorldObjectData* object = GetObjectData(objectIndex);

		if (object != nullptr) {
			if (object->id == identifier) {
				return object;
			}
		}
	}

	return nullptr;
}

// 子Colliderを最も近い有効な親Rigidbodyへ結び付ける。0は固定Collider。
WorldObjectId IWorldObjects::GetBodyOwner(WorldObjectId identifier) const {
	// 親をたどり、Colliderが属する最も近い有効なRigidbodyを探す。
	for (size_t depth = 0; depth < 64; ++depth) {
		const WorldObjectData* object = GetObjectDataById(identifier);

		if (object == nullptr) {
			return 0;
		}

		if (object->body.isPresent) {
			if (object->body.isEnabled) {
				return object->id;
			}
		}

		identifier = object->parentId;
	}

	return 0;
}

// 採番済みIDはUndoや削除の後にも再利用しない。未対応実装は最大IDから求める。
WorldObjectId IWorldObjects::GetNextObjectId() const {
	// 次の作成に使用する未使用IDを取得する。
	WorldObjectId nextIdentifier = 1;

	for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
		const WorldObjectData* object = GetObjectData(objectIndex);

		if (object != nullptr) {
			if (object->id >= nextIdentifier) {
				nextIdentifier = object->id + 1;
			}
		}
	}

	return nextIdentifier;
}

// 親子変換は保存データと共通の行列で扱う。GPU側の参照は公開しない。
bool IWorldObjects::GetObjectWorldMatrix(WorldObjectId, Matrix4x4&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

Vector3 IWorldObjects::GetObjectWorldPosition(WorldObjectId identifier) const {
	// ワールド行列の平行移動成分から位置を取り出す。
	Matrix4x4 world{};

	if (!GetObjectWorldMatrix(identifier, world)) {
		return {};
	}

	return {world.m[3][0], world.m[3][1], world.m[3][2]};
}

//=============================================================================================//
// Getter・Setter / Transform・親子関係

bool IWorldObjects::SetObjectWorldTransform(WorldObjectId, const Matrix4x4&) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

bool IWorldObjects::SetObjectWorldPosition(WorldObjectId, const Vector3&) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

bool IWorldObjects::IsObjectActive(WorldObjectId) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

bool IWorldObjects::IsColliderTouching(WorldObjectId) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

bool IWorldObjects::SetObjectParent(WorldObjectId, WorldObjectId, std::string& error) {
	// 既定実装では配置の変更に対応しないことを通知する。
	error = "This scene does not support parenting.";

	return false;
}

//=============================================================================================//
// Getter・Setter / ゲームカメラ

// メイン指定と投影をCPU側で解決し、GameViewと実行時で共有する。
bool IWorldObjects::SetMainCamera(WorldObjectId) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

WorldObjectId IWorldObjects::GetMainCameraId() const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return kInvalidWorldObjectId;
}

// メイン指定に関係なく、指定Cameraの実描画と同じ視野を取得する。
bool IWorldObjects::GetCameraView(WorldObjectId, float, CameraView&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

bool IWorldObjects::GetGameCameraView(float, CameraView&) const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

//=============================================================================================//
// Getter / 一覧の保存・復元

bool IWorldObjects::HasObjectCollectionSupport() const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

std::vector<WorldObjectData> IWorldObjects::CaptureObjects() const {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return {};
}

//=============================================================================================//
// 作成・配置・置換

// Prefabを新しいIDの独立した階層として追加する。
WorldObjectId IWorldObjects::PlacePrefab(const std::string&, WorldObjectId, const Vector3&, std::string&) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return 0;
}

// 配置に必要な変換・親・モデルを先に検証し、失敗時は一覧を変更しない。
WorldObjectId IWorldObjects::PlaceModel(const std::string&, WorldObjectId, const Vector3&, std::string& error) {
	// 既定実装では配置の変更に対応しないことを通知する。
	error = "This scene does not support model placement.";

	return kInvalidWorldObjectId;
}

WorldObjectId IWorldObjects::AddObject(const std::string&) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return kInvalidWorldObjectId;
}

WorldObjectId IWorldObjects::CreateObject(WorldObjectType) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return kInvalidWorldObjectId;
}

WorldObjectId IWorldObjects::DuplicateObject(WorldObjectId) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return kInvalidWorldObjectId;
}

bool IWorldObjects::RemoveObject(WorldObjectId) {
	// 既定実装はこの機能を持たないため、利用できる対象や値がないことを返す。
	return false;
}

// 実装側は全件を検証してから置換し、失敗時は現在の一覧を維持する。
bool IWorldObjects::ReplaceObjects(std::vector<WorldObjectData>, std::string& error) {
	// 既定実装では配置の変更に対応しないことを通知する。
	error = "This scene does not support object replacement.";

	return false;
}
