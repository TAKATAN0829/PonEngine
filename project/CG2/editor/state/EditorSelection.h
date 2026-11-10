#pragma once

#include "IWorldObjects.h"
#include <algorithm>
#include <unordered_set>

// IDだけを所有し、末尾を主選択として単体Inspectorとの互換性を保つ。
class EditorSelection {
public:

	//=============================================================================================//
	// Getter / 選択一覧・主選択

	// 最後に選択した主対象のID。未選択なら無効IDを返す。
	WorldObjectId GetPrimarySelectedObjectId() const;

	const std::vector<WorldObjectId>& GetIds() const;

	bool DoesContain(WorldObjectId identifier) const;

	// 親も選択されている子を除き、二重移動・二重複製を防ぐ。
	std::vector<WorldObjectId> GetRoots(const IWorldObjects& objects) const;

	//=============================================================================================//
	// Setter / 単体選択・追加選択・解除

	void Select(WorldObjectId identifier);

	void Toggle(WorldObjectId identifier);

	void Clear();

	// Undo・削除後は、消えたIDだけを選択一覧から除く。
	void Prune(const IWorldObjects& objects);

	//=============================================================================================//
	// 一括複製・削除

	// 一時シーンで上限を検証してから反映し、途中失敗で部分変更しない。
	bool Duplicate(IWorldObjects& objects, std::string& error);

	bool Remove(IWorldObjects& objects, std::string& error);

private:

	//=============================================================================================//
	// 選択ID

	std::vector<WorldObjectId> selected_; // フレーム内の借用参照は保持しない。

	std::unordered_set<WorldObjectId> selectedIds_; // Hierarchy各行の選択判定を一定時間で行う。
};
