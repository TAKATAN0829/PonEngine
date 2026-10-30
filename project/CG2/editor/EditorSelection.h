#pragma once
#include "IEditableScene.h"
#include <algorithm>
#include <unordered_set>

// IDだけを所有し、末尾を主選択として単体Inspectorとの互換性を保つ。
class EditorSelection {
public:
	//=============================================================================================//
	// Getter / 選択一覧・主選択

	SceneObjectId Get() const {
		if (selected_.empty()) {
			return kInvalidSceneObjectId;
		}
		return selected_.back();
	}
	const std::vector<SceneObjectId>& GetIds() const { return selected_; }
	bool Contains(SceneObjectId identifier) const {
		return selectedIds_.contains(identifier);
	}
	// 親も選択されている子を除き、二重移動・二重複製を防ぐ。
	std::vector<SceneObjectId> GetRoots(const IEditableScene& scene) const {
		std::vector<SceneObjectId> roots;
		for (SceneObjectId identifier : selected_) {
			const SceneObjectData* object = scene.GetObjectDataById(identifier);
			if (object == nullptr) {
				continue;
			}
			SceneObjectId ancestor = object->parentId;
			while (ancestor != 0) {
				if (Contains(ancestor)) {
					break;
				}
				const SceneObjectData* parent = scene.GetObjectDataById(ancestor);
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

	void Select(SceneObjectId identifier) {
		Clear();
		if (identifier != 0) {
			selected_.push_back(identifier);
			selectedIds_.insert(identifier);
		}
	}
	void Toggle(SceneObjectId identifier) {
		if (identifier == 0) {
			return;
		}
		const std::vector<SceneObjectId>::iterator found = std::find(selected_.begin(), selected_.end(), identifier);
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
	void Prune(const IEditableScene& scene) {
		for (size_t selectionIndex = selected_.size(); selectionIndex > 0; --selectionIndex) {
			if (scene.GetObjectDataById(selected_[selectionIndex - 1]) == nullptr) {
				selectedIds_.erase(selected_[selectionIndex - 1]);
				selected_.erase(selected_.begin() + selectionIndex - 1);
			}
		}
	}

	//=============================================================================================//
	// メンバ関数 / 一括複製・削除

	// 一時シーンで上限を検証してから反映し、途中失敗で部分変更しない。
	bool Duplicate(IEditableScene& scene, std::string& error);
	bool Remove(IEditableScene& scene, std::string& error);

private:
	//=============================================================================================//
	// メンバ変数 / 選択ID

	std::vector<SceneObjectId> selected_; // フレーム内の借用参照は保持しない。
	std::unordered_set<SceneObjectId> selectedIds_; // Hierarchy各行の選択判定を一定時間で行う。
};
