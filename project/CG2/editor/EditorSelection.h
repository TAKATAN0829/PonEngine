#pragma once
#include "IEditableWorld.h"
#include <algorithm>
#include <unordered_set>

// IDだけを所有し、末尾を主選択として単体Inspectorとの互換性を保つ。
class EditorSelection {
public:
	//=============================================================================================//
	// Getter / 選択一覧・主選択

	WorldObjectId Get() const {
		if (selected_.empty()) {
			return kInvalidWorldObjectId;
		}
		return selected_.back();
	}
	const std::vector<WorldObjectId>& GetIds() const { return selected_; }
	bool Contains(WorldObjectId identifier) const {
		return selectedIds_.contains(identifier);
	}
	// 親も選択されている子を除き、二重移動・二重複製を防ぐ。
	std::vector<WorldObjectId> GetRoots(const IEditableWorld& scene) const {
		std::vector<WorldObjectId> roots;
		for (WorldObjectId identifier : selected_) {
			const WorldObjectData* object = scene.GetObjectDataById(identifier);
			if (object == nullptr) {
				continue;
			}
			WorldObjectId ancestor = object->parentId;
			while (ancestor != 0) {
				if (Contains(ancestor)) {
					break;
				}
				const WorldObjectData* parent = scene.GetObjectDataById(ancestor);
				if (parent == nullptr) {
					break;
				}
				ancestor = parent->parentId;
			}
			if (ancestor == 0) {
				roots.push_back(identifier);
			}
		}
		return roots;
	}

	//=============================================================================================//
	// Setter / 単体選択・追加選択・解除

	void Select(WorldObjectId identifier) {
		Clear();
		if (identifier != 0) {
			selected_.push_back(identifier);
			selectedIds_.insert(identifier);
		}
	}
	void Toggle(WorldObjectId identifier) {
		if (identifier == 0) {
			return;
		}
		const std::vector<WorldObjectId>::iterator found = std::find(selected_.begin(), selected_.end(), identifier);
		if (found == selected_.end()) {
			selected_.push_back(identifier);
			selectedIds_.insert(identifier);
		} else {
			selected_.erase(found);
			selectedIds_.erase(identifier);
		}
	}
	void Clear() {
		selected_.clear();
		selectedIds_.clear();
	}
	// Undo・削除後は、消えたIDだけを選択一覧から除く。
	void Prune(const IEditableWorld& scene) {
		for (size_t selectionIndex = selected_.size(); selectionIndex > 0; --selectionIndex) {
			if (scene.GetObjectDataById(selected_[selectionIndex - 1]) == nullptr) {
				selectedIds_.erase(selected_[selectionIndex - 1]);
				selected_.erase(selected_.begin() + selectionIndex - 1);
			}
		}
	}

	//=============================================================================================//
	// 一括複製・削除

	// 一時シーンで上限を検証してから反映し、途中失敗で部分変更しない。
	bool Duplicate(IEditableWorld& scene, std::string& error);
	bool Remove(IEditableWorld& scene, std::string& error);

private:
	//=============================================================================================//
	// メンバ変数 / 選択ID

	std::vector<WorldObjectId> selected_; // フレーム内の借用参照は保持しない。
	std::unordered_set<WorldObjectId> selectedIds_; // Hierarchy各行の選択判定を一定時間で行う。
};
