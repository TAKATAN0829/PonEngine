#pragma once

#include "IEditableScene.h"
#include <unordered_map>

// 編集データを所有する。追加・削除しても残るオブジェクトのIDは変更しない。
class SceneObjects : public IEditableScene {
public:

	//=============================================================================================//
	// 定数 / オブジェクト上限

	static constexpr size_t kMaxObjects = 10000;

	//=============================================================================================//
	// Getter / オブジェクト一覧

	size_t GetEditableObjectCount() const override { return objects_.size(); }
	EditableObjectView GetEditableObject(size_t index) override;
	const SceneObjectData* GetObjectData(size_t index) const override;
	const std::vector<SceneObjectData>& GetObjects() const { return objects_; }
	std::vector<SceneObjectData> CaptureObjects() const override { return objects_; }
	bool SupportsObjectCollection() const override { return true; }

	//=============================================================================================//
	// Getter・Setter / 親子変換

	bool GetObjectWorldMatrix(SceneObjectId id, Matrix4x4& world) const override;
	bool SetObjectWorldPosition(SceneObjectId id, const Vector3& position) override;
	bool IsObjectActive(SceneObjectId id) const override;
	// ワールド変換を維持し、循環・未登録の親・TRSで表せない変換は拒否する。
	bool SetObjectParent(SceneObjectId childId, SceneObjectId parentId, std::string& error) override;


	//=============================================================================================//
	// Getter・Setter / メインカメラ

	bool SetMainCamera(SceneObjectId id) override;
	SceneObjectId GetMainCameraId() const override;
	bool GetGameCameraView(float aspectRatio, SceneCameraView& view) const override;

	//=============================================================================================//
	// メンバ関数 / 追加・複製・削除

	SceneObjectId AddObject(const std::string& model) override;
	SceneObjectId CreateObject(SceneObjectType type) override;
	SceneObjectId DuplicateObject(SceneObjectId id) override;
	bool RemoveObject(SceneObjectId id) override;

	//=============================================================================================//
	// メンバ関数 / 一括置換

	// 全件の検証後に反映し、失敗時は現在の一覧を維持する。
	bool ReplaceObjects(std::vector<SceneObjectData> objects, std::string& error) override;

	//=============================================================================================//
	// メンバ関数 / 保存データ検証

	static bool IsSupportedModel(const std::string& model);
	static bool Validate(const SceneObjectData& object);

private:

	//=============================================================================================//
	// Getter / 検索・追加可否

	bool CanCreateType(SceneObjectType type) const;
	size_t FindObjectIndex(SceneObjectId objectId) const;
	bool IsDescendantOf(SceneObjectId objectId, SceneObjectId ancestorId) const;
	void RebuildObjectIndices();
	// 親の存在・循環・深さを検証し、不正な保存データを部分反映しない。
	static bool ValidateHierarchy(const std::vector<SceneObjectData>& objects, const std::unordered_map<SceneObjectId, size_t>& indices, std::string& error);

private:

	//=============================================================================================//
	// メンバ変数 / 編集データ

	std::unordered_map<SceneObjectId, size_t> objectIndices_; // ID検索をオブジェクト数に比例させない。
	std::vector<SceneObjectData> objects_; // GPUリソースは派生シーン側で別に管理する。

	//=============================================================================================//
	// メンバ変数 / ID採番

	SceneObjectId nextId_ = 1; // Undo・読込で巻き戻さず、削除済みIDを再利用しない。
};
