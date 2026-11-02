#pragma once
#include "objects/SceneObjectData.h"
#include <cstddef>

// 実行・保存・Editorで共有するオブジェクト操作の約束。
// カメラ入力・編集画面・Assetプレビューには依存せず、データの所有権も取得しない。
class ISceneObjects {
public:
	//=============================================================================================//
	// 終了処理

	virtual ~ISceneObjects() = default;

	//=============================================================================================//
	// Getter / オブジェクト一覧・ID

	virtual size_t GetEditableObjectCount() const = 0;
	// 返す参照は追加・削除・読込で無効になるため、フレームをまたいで保持しない。
	virtual EditableObjectView GetEditableObject(size_t index) = 0;
	// 履歴比較用の読み取り専用参照。一覧変更後には取得し直す。
	// 公開できないシーンはnullptrを返し、CaptureObjectsでの比較へ切り替える。
	virtual const SceneObjectData* GetObjectData(size_t index) const { (void)index; return nullptr; }
	// ID検索を公開し、選択の検証で毎フレーム一覧のコピーを作らない。
	virtual const SceneObjectData* GetObjectDataById(SceneObjectId identifier) const {
		for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
			const SceneObjectData* object = GetObjectData(objectIndex);
			if (object != nullptr) {
				if (object->id == identifier) {
					return object;
				}
			}
		}
		return nullptr;
	}
	// 子Colliderを最も近い有効な親Rigidbodyへ結び付ける。0は固定Collider。
	SceneObjectId GetBodyOwner(SceneObjectId identifier) const {
		for (size_t depth = 0; depth < 64; ++depth) {
			const SceneObjectData* object = GetObjectDataById(identifier);
			if (object == nullptr) {
				return 0;
			}
			if (object->body.present) {
				if (object->body.enabled) {
					return object->id;
				}
			}
			identifier = object->parentId;
		}
		return 0;
	}
	// 採番済みIDはUndoや削除の後にも再利用しない。未対応実装は最大IDから求める。
	virtual SceneObjectId GetNextObjectId() const {
		SceneObjectId nextIdentifier = 1;
		for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
			const SceneObjectData* object = GetObjectData(objectIndex);
			if (object != nullptr) {
				if (object->id >= nextIdentifier) {
					nextIdentifier = object->id + 1;
				}
			}
		}
		return nextIdentifier;
	}
	// 親子変換は保存データと共通の行列で扱う。GPU側の参照は公開しない。
	virtual bool GetObjectWorldMatrix(SceneObjectId, Matrix4x4&) const { return false; }
	Vector3 GetObjectWorldPosition(SceneObjectId id) const {
		Matrix4x4 world{};
		if (!GetObjectWorldMatrix(id, world)) {
			return {};
		}
		return {world.m[3][0], world.m[3][1], world.m[3][2]};
	}
	//=============================================================================================//
	// Getter・Setter / Transform・親子関係

	virtual bool SetObjectWorldTransform(SceneObjectId, const Matrix4x4&) { return false; }
	virtual bool SetObjectWorldPosition(SceneObjectId, const Vector3&) { return false; }
	virtual bool IsObjectActive(SceneObjectId) const { return false; }
	virtual bool IsColliderTouching(SceneObjectId) const { return false; }
	virtual bool SetObjectParent(SceneObjectId, SceneObjectId, std::string& error) {
		error = "This scene does not support parenting.";
		return false;
	}
	//=============================================================================================//
	// Getter・Setter / ゲームカメラ

	// メイン指定と投影をCPU側で解決し、GameViewと実行時で共有する。
	virtual bool SetMainCamera(SceneObjectId) { return false; }
	virtual SceneObjectId GetMainCameraId() const { return kInvalidSceneObjectId; }
	// メイン指定に関係なく、指定Cameraの実描画と同じ視野を取得する。
	virtual bool GetCameraView(SceneObjectId, float, SceneCameraView&) const { return false; }
	virtual bool GetGameCameraView(float, SceneCameraView&) const { return false; }
	//=============================================================================================//
	// Getter / 一覧の保存・復元

	virtual bool SupportsObjectCollection() const { return false; }
	virtual std::vector<SceneObjectData> CaptureObjects() const { return {}; }

	//=============================================================================================//
	// メンバ関数 / 作成・配置・置換

	// Prefabを新しいIDの独立した階層として追加する。
	virtual SceneObjectId PlacePrefab(const std::string&, SceneObjectId, const Vector3&, std::string&) { return 0; }
	// 配置に必要な変換・親・モデルを先に検証し、失敗時は一覧を変更しない。
	virtual SceneObjectId PlaceModel(const std::string&, SceneObjectId, const Vector3&, std::string& error) {
		error = "This scene does not support model placement.";
		return kInvalidSceneObjectId;
	}
	virtual SceneObjectId AddObject(const std::string&) { return kInvalidSceneObjectId; }
	virtual SceneObjectId CreateObject(SceneObjectType) { return kInvalidSceneObjectId; }
	virtual SceneObjectId DuplicateObject(SceneObjectId) { return kInvalidSceneObjectId; }
	virtual bool RemoveObject(SceneObjectId) { return false; }
	// 実装側は全件を検証してから置換し、失敗時は現在の一覧を維持する。
	virtual bool ReplaceObjects(std::vector<SceneObjectData>, std::string& error) {
		error = "This scene does not support object replacement.";
		return false;
	}
};
