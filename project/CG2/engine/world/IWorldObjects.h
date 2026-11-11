#pragma once

#include "objects/WorldObjectData.h"
#include <cstddef>

// 実行・保存・Editorで共有するオブジェクト操作の約束。
// カメラ入力・編集画面・Assetプレビューには依存せず、データの所有権も取得しない。
class IWorldObjects {
public:

	//=============================================================================================//
	// 終了処理

	virtual ~IWorldObjects() = default;

	//=============================================================================================//
	// Getter / オブジェクト一覧・ID

	virtual size_t GetEditableObjectCount() const = 0;

	// 返す参照は追加・削除・読込で無効になるため、フレームをまたいで保持しない。
	virtual EditableObjectView GetEditableObject(size_t index) = 0;

	// 履歴比較用の読み取り専用参照。一覧変更後には取得し直す。
	// 公開できないシーンはnullptrを返し、CaptureObjectsでの比較へ切り替える。
	virtual const WorldObjectData* GetObjectData(size_t index) const;

	// ID検索を公開し、選択の検証で毎フレーム一覧のコピーを作らない。
	virtual const WorldObjectData* GetObjectDataById(WorldObjectId identifier) const;

	// 子Colliderを最も近い有効な親Rigidbodyへ結び付ける。0は固定Collider。
	WorldObjectId GetBodyOwner(WorldObjectId identifier) const;

	// 採番済みIDはUndoや削除の後にも再利用しない。未対応実装は最大IDから求める。
	virtual WorldObjectId GetNextObjectId() const;

	// 親子変換は保存データと共通の行列で扱う。GPU側の参照は公開しない。
	virtual bool GetObjectWorldMatrix(WorldObjectId, Matrix4x4&) const;

	Vector3 GetObjectWorldPosition(WorldObjectId identifier) const;

	//=============================================================================================//
	// Getter・Setter / Transform・親子関係

	virtual bool SetObjectWorldTransform(WorldObjectId, const Matrix4x4&);

	virtual bool SetObjectWorldPosition(WorldObjectId, const Vector3&);

	virtual bool IsObjectActive(WorldObjectId) const;

	virtual bool IsColliderTouching(WorldObjectId) const;

	virtual bool SetObjectParent(WorldObjectId, WorldObjectId, std::string& error);

	//=============================================================================================//
	// Getter・Setter / ゲームカメラ

	// メイン指定と投影をCPU側で解決し、GameViewと実行時で共有する。
	virtual bool SetMainCamera(WorldObjectId);

	virtual WorldObjectId GetMainCameraId() const;

	// メイン指定に関係なく、指定Cameraの実描画と同じ視野を取得する。
	virtual bool GetCameraView(WorldObjectId, float, CameraView&) const;

	//=============================================================================================//
	// Getter / 一覧の保存・復元

	virtual bool HasObjectCollectionSupport() const;

	virtual std::vector<WorldObjectData> CaptureObjects() const;

	//=============================================================================================//
	// 作成・配置・置換

	// 配置に必要な変換・親・モデルを先に検証し、失敗時は一覧を変更しない。
	virtual WorldObjectId PlaceModel(const std::string&, WorldObjectId, const Vector3&, std::string& error);

	virtual WorldObjectId AddObject(const std::string&);

	virtual WorldObjectId CreateObject(WorldObjectType);

	virtual WorldObjectId DuplicateObject(WorldObjectId);

	virtual bool RemoveObject(WorldObjectId);

	// 実装側は全件を検証してから置換し、失敗時は現在の一覧を維持する。
	virtual bool ReplaceObjects(std::vector<WorldObjectData>, std::string& error);
};
