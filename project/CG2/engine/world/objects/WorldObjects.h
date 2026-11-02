#pragma once

#include "IEditableWorld.h"
#include <unordered_map>

// 編集データを所有する。追加・削除しても残るオブジェクトのIDは変更しない。
class WorldObjects : public IEditableWorld {
public:

	//=============================================================================================//
	// 定数 / オブジェクト上限

	static constexpr size_t kMaxObjects = 10000;

	//=============================================================================================//
	// Getter / オブジェクト一覧

	size_t GetEditableObjectCount() const override { return objects_.size(); }

	EditableObjectView GetEditableObject(size_t index) override;

	const WorldObjectData* GetObjectData(size_t index) const override;
	
	// 既存のID検索表を使い、複数選択・親参照の確認を一定時間で行う。
	const WorldObjectData* GetObjectDataById(WorldObjectId identifier) const override;

	WorldObjectId GetNextObjectId() const override { return nextId_; }

	const std::vector<WorldObjectData>& GetObjects() const { return objects_; }

	std::vector<WorldObjectData> CaptureObjects() const override { return objects_; }

	bool SupportsObjectCollection() const override { return true; }

	//=============================================================================================//
	// Getter・Setter / 親子変換

	bool GetObjectWorldMatrix(WorldObjectId id, Matrix4x4& world) const override;

	// 親を考慮してローカルTRSへ戻す。せん断や特異な変換では編集しない。
	bool SetObjectWorldTransform(WorldObjectId id, const Matrix4x4& world) override;

	// resources内のPrefabを検証して一括配置する。
	WorldObjectId PlacePrefab(const std::string& prefab, WorldObjectId parentId, const Vector3& position, std::string& error) override;

	bool SetObjectWorldPosition(WorldObjectId id, const Vector3& position) override;

	bool IsObjectActive(WorldObjectId id) const override;

	// ワールド変換を維持し、循環・未登録の親・TRSで表せない変換は拒否する。
	bool SetObjectParent(WorldObjectId childId, WorldObjectId parentId, std::string& error) override;


	//=============================================================================================//
	// Getter・Setter / メインカメラ

	bool SetMainCamera(WorldObjectId id) override;

	WorldObjectId GetMainCameraId() const override;

	bool GetGameCameraView(float aspectRatio, CameraView& view) const override;

	// 選択中カメラの補助線にも、実描画と同じ拡縮除去を適用する。
	bool GetCameraView(WorldObjectId cameraId, float aspectRatio, CameraView& view) const override;

	//=============================================================================================//
	// メンバ関数 / 追加・複製・削除

	WorldObjectId PlaceModel(const std::string& model, WorldObjectId parentId, const Vector3& position, std::string& error) override;
	WorldObjectId AddObject(const std::string& model) override;
	WorldObjectId CreateObject(WorldObjectType type) override;
	WorldObjectId DuplicateObject(WorldObjectId id) override;
	bool RemoveObject(WorldObjectId id) override;

	//=============================================================================================//
	// メンバ関数 / 一括置換

	// 全件の検証後に反映し、失敗時は現在の一覧を維持する。
	bool ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error) override;

	//=============================================================================================//
	// メンバ関数 / 保存データ検証

	static bool IsSupportedModel(const std::string& model);
	static bool Validate(const WorldObjectData& object);

private:

	//=============================================================================================//
	// Getter / 検索・追加可否

	bool CanCreateType(WorldObjectType type) const;

	size_t FindObjectIndex(WorldObjectId objectId) const;

	bool IsDescendantOf(WorldObjectId objectId, WorldObjectId ancestorId) const;

	void RebuildObjectIndices();

	// 親の存在・循環・深さを検証し、不正な保存データを部分反映しない。
	static bool ValidateHierarchy(const std::vector<WorldObjectData>& objects, const std::unordered_map<WorldObjectId, size_t>& indices, std::string& error);

private:

	//=============================================================================================//
	// メンバ変数 / 編集データ

	std::unordered_map<WorldObjectId, size_t> objectIndices_; // ID検索をオブジェクト数に比例させない。
	std::vector<WorldObjectData> objects_; // GPUリソースは派生シーン側で別に管理する。

	//=============================================================================================//
	// メンバ変数 / ID採番

	WorldObjectId nextId_ = 1; // Undo・読込で巻き戻さず、削除済みIDを再利用しない。
};
