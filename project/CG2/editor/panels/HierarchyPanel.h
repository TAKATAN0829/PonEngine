#pragma once

#include "IEditableScene.h"
#include <string>
#include <unordered_map>

class EditorSelection;

// 親子ツリーの表示と追加・複製・削除・親の変更を担当する。
class HierarchyPanel {
public:
	//=============================================================================================//
	// 初期化処理

	static void Initialize();

	//=============================================================================================//
	// 描画処理

	static void Draw(IEditableScene* scene, EditorSelection& selection);

private:
	//=============================================================================================//
	// 型 / 親子一覧・操作要求

	using ChildIndices = std::unordered_map<SceneObjectId, std::vector<size_t>>;
	struct HierarchyRequest {
		SceneObjectId childId = kInvalidSceneObjectId;
		SceneObjectId parentId = kInvalidSceneObjectId;
		bool requested = false;
		std::string model; // Projectからのモデル配置要求。
		bool prefab = false; // 同じ固定長パスをPrefab配置にも使う。
		bool contextRequested = false; // 行のIDスコープを抜けてからメニューを開く。
		SceneObjectId contextObjectId = kInvalidSceneObjectId;
	};

	//=============================================================================================//
	// メンバ関数 / 操作UI・ツリー

	static void DrawCreateMenu(IEditableScene& scene, EditorSelection& selection);
	static void DrawContextMenu(IEditableScene& scene, EditorSelection& selection);
	static void DrawPrefabSaveMenu(IEditableScene& scene);
	static void DrawBranch(IEditableScene& scene, EditorSelection& selection, const ChildIndices& children,
		SceneObjectId parentId, HierarchyRequest& request);
	// 描画中は参照を無効化しないよう、ドロップ結果だけを記録する。
	static void AcceptParentDrop(SceneObjectId parentId, HierarchyRequest& request);

private:
	//=============================================================================================//
	// メンバ変数 / 操作結果

	inline static SceneObjectId contextTarget_ = kInvalidSceneObjectId; // 右クリックした対象。
	inline static std::string status_; // シーン切替時に初期化する。
	inline static char prefabName_[128] = "Prefab"; // 保存先のファイル名だけを入力する。
};
