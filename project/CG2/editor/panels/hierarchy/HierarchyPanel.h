#pragma once

#include "IEditableWorld.h"
#include <string>
#include <unordered_map>

class EditorSelection;

// 親子ツリーの表示と追加・複製・削除・親の変更を担当する。
class HierarchyPanel {
public:

	//=============================================================================================//
	// 初期化処理

	static void Initialize ();

	//=============================================================================================//
	// 描画処理

	static void Draw (IEditableWorld* scene, EditorSelection& selection);

private:

	//=============================================================================================//
	// 型 / 親子一覧・操作要求

	using ChildIndices = std::unordered_map<WorldObjectId, std::vector<size_t>>;

	struct HierarchyRequest {
		WorldObjectId childId = kInvalidWorldObjectId;
		WorldObjectId parentId = kInvalidWorldObjectId;

		bool isRequested = false;

		std::string model; // Projectからのモデル配置要求。

		bool isPrefab = false; // 同じ固定長パスをPrefab配置にも使う。
		bool isContextRequested = false; // 行のIDスコープを抜けてからメニューを開く。

		WorldObjectId contextObjectId = kInvalidWorldObjectId;
	};

	//=============================================================================================//
	// 操作UI・ツリー

	static void DrawCreateMenu (IEditableWorld& scene, EditorSelection& selection);

	static void DrawContextMenu (IEditableWorld& scene, EditorSelection& selection);

	static void DrawPrefabSaveMenu (IEditableWorld& scene);

	static void DrawBranch (IEditableWorld& scene, EditorSelection& selection, const ChildIndices& children, WorldObjectId parentId, HierarchyRequest& request);

	// 描画中は参照を無効化しないよう、ドロップ結果だけを記録する。
	static void AcceptParentDrop (WorldObjectId parentId, HierarchyRequest& request);

private:

	//=============================================================================================//
	// 操作結果

	inline static WorldObjectId contextTarget_ = kInvalidWorldObjectId; // 右クリックした対象。

	inline static std::string status_; // シーン切替時に初期化する。

	inline static char prefabName_[128] = "Prefab"; // 保存先のファイル名だけを入力する。
};
