#include "EditorSelection.h"
#include "objects/WorldObjects.h"
#include <limits>
#include <unordered_map>

//=============================================================================================//
// 実装内補助定義

namespace {
	// 選択ルートとその子孫を一度だけ列挙する。一括操作中は所有スナップショットを使う。
	std::unordered_set<WorldObjectId> CollectSubtrees(const std::vector<WorldObjectData>& objects,
		const std::vector<WorldObjectId>& roots) {
		// 親IDから子を引ける一覧を作る。
		std::unordered_map<WorldObjectId, std::vector<WorldObjectId>> children;

		for (const WorldObjectData& object : objects) {
			children[object.parentId].push_back(object.id);
		}

		// 処理済みのIDを記録し、重複を避ける。
		std::unordered_set<WorldObjectId> included;

		// 選択ルートから子孫へ探索する。
		std::vector<WorldObjectId> pending = roots;

		while (!pending.empty()) {
			// 未処理のIDを一件取り出す。
			const WorldObjectId identifier = pending.back();

			pending.pop_back();

			// 同じオブジェクトは一度だけ処理する。
			if (!included.insert(identifier).second) {
				continue;
			}

			// 子があれば次の探索対象へ追加する。
			const std::unordered_map<WorldObjectId, std::vector<WorldObjectId>>::const_iterator found = children.find(identifier);

			if (found != children.end()) {
				pending.insert(pending.end(), found->second.begin(), found->second.end());
			}
		}

		return included;
	}
}

//=============================================================================================//
// Getter / 選択一覧・主選択

WorldObjectId EditorSelection::GetPrimarySelectedObjectId() const {
	// 未選択なら無効IDを返す。
	if (selected_.empty()) {
		return kInvalidWorldObjectId;
	}

	// 最後に選んだオブジェクトを主対象にする。
	return selected_.back();
}

const std::vector<WorldObjectId>& EditorSelection::GetIds() const {
	// 選択順を保ったID一覧を参照で返す。
	return selected_;
}

bool EditorSelection::DoesContain(WorldObjectId identifier) const {
	// 検索用の集合で選択状態を確認する。
	return selectedIds_.contains(identifier);
}

// 親も選択されている子を除き、二重移動・二重複製を防ぐ。
std::vector<WorldObjectId> EditorSelection::GetRoots(const IWorldObjects& objects) const {
	// 親も選択されている子は結果へ含めない。
	std::vector<WorldObjectId> roots;

	for (WorldObjectId identifier : selected_) {
		// すでに削除された選択は飛ばす。
		const WorldObjectData* object = objects.GetObjectDataById(identifier);

		if (object == nullptr) {
			continue;
		}

		// 選択済みの祖先があるか、親をたどる。
		WorldObjectId ancestor = object->parentId;

		while (ancestor != 0) {
			if (DoesContain(ancestor)) {
				break;
			}

			const WorldObjectData* parent = objects.GetObjectDataById(ancestor);

			if (parent == nullptr) {
				break;
			}

			ancestor = parent->parentId;
		}

		// 選択済みの祖先がなければ操作のルートにする。
		if (ancestor == 0) {
			roots.push_back(identifier);
		}
	}

	return roots;
}

//=============================================================================================//
// Setter / 単体選択・追加選択・解除

void EditorSelection::Select(WorldObjectId identifier) {
	// 前の選択を外し、単体選択へ切り替える。
	Clear();

	// 無効IDは選択一覧に入れない。
	if (identifier != 0) {
		// 選択順と検索用の集合を揃えて更新する。
		selected_.push_back(identifier);

		selectedIds_.insert(identifier);
	}
}

void EditorSelection::Toggle(WorldObjectId identifier) {
	// 無効IDの切替要求は無視する。
	if (identifier == 0) {
		return;
	}

	// 一覧に含まれている位置を調べる。
	const std::vector<WorldObjectId>::iterator found = std::find(selected_.begin(), selected_.end(), identifier);

	// 未選択なら追加し、選択済みなら解除する。
	if (found == selected_.end()) {
		selected_.push_back(identifier);

		selectedIds_.insert(identifier);
	} else {
		selected_.erase(found);

		selectedIds_.erase(identifier);
	}
}

void EditorSelection::Clear() {
	// 選択順と検索用の集合を両方空にする。
	selected_.clear();

	selectedIds_.clear();
}

// Undo・削除後は、消えたIDだけを選択一覧から除く。
void EditorSelection::Prune(const IWorldObjects& objects) {
	// 削除で添字がずれないよう、末尾から確認する。
	for (size_t selectionIndex = selected_.size(); selectionIndex > 0; --selectionIndex) {
		// 実体がないIDは両方の一覧から取り除く。
		if (objects.GetObjectDataById(selected_[selectionIndex - 1]) == nullptr) {
			selectedIds_.erase(selected_[selectionIndex - 1]);

			selected_.erase(selected_.begin() + selectionIndex - 1);
		}
	}
}

//=============================================================================================//
// 一括複製・削除


// 選択数分のシーン全体コピーを避け、採番・親の付け替え・検証を一回で行う。
bool EditorSelection::Duplicate(IWorldObjects& objects, std::string& error) {
	// 今回の複製処理の結果だけを返す。
	error.clear();

	// 二重複製を避けるため、選択のルートを求める。
	const std::vector<WorldObjectId> roots = GetRoots(objects);

	// 対象がなければ配置を変更しない。
	if (roots.empty()) {
		return false;
	}

	// 元の配置を保持したまま、複製対象の子孫を集める。
	const std::vector<WorldObjectData> original = objects.CaptureObjects();

	const std::unordered_set<WorldObjectId> included = CollectSubtrees(original, roots);

	// 複製後のオブジェクト数を先に検証する。
	if (original.size() + included.size() > WorldObjects::kMaxObjects) {
		error = "Cannot duplicate selection: object limit reached.";

		return false;
	}

	// 未使用のIDから、複製分の採番を始める。
	WorldObjectId nextIdentifier = objects.GetNextObjectId();

	// IDの上限を超える複製を防ぐ。
	if (included.size() > (std::numeric_limits<WorldObjectId>::max)() - nextIdentifier) {
		error = "Cannot duplicate selection: ID limit reached.";

		return false;
	}

	// 元IDと複製後のIDの対応を作る。
	std::unordered_map<WorldObjectId, WorldObjectId> remapped;

	for (const WorldObjectData& object : original) {
		if (included.contains(object.id)) {
			remapped[object.id] = nextIdentifier++;
		}
	}

	// 元の配置へ複製分を追加する準備をする。
	std::vector<WorldObjectData> candidate = original;

	candidate.reserve(original.size() + included.size());

	for (const WorldObjectData& object : original) {
		// 対象の階層だけを複製する。
		if (!included.contains(object.id)) {
			continue;
		}

		WorldObjectData copy = object;

		// 複製したオブジェクトに新しいIDを設定する。
		copy.id = remapped.at(object.id);

		// 親も複製対象なら、新しい親IDへ接続する。
		if (remapped.contains(copy.parentId)) {
			copy.parentId = remapped.at(copy.parentId);
		}

		// 複製でメインカメラが重複しないようにする。
		copy.isMainCamera = false;

		// 識別用の接尾辞を付ける前に、長い名前を短くする。
		if (copy.name.size() > 200) {
			copy.name = "Object";
		}

		copy.name += " Copy #" + std::to_string(copy.id);

		candidate.push_back(std::move(copy));
	}

	// 全件の検証に成功した場合だけ配置を置き換える。
	if (!objects.ReplaceObjects(std::move(candidate), error)) {
		return false;
	}

	// 複製したルートを新しい選択対象にする。
	Clear();

	for (WorldObjectId identifier : roots) {
		selected_.push_back(remapped.at(identifier));

		selectedIds_.insert(remapped.at(identifier));
	}

	return true;
}

bool EditorSelection::Remove(IWorldObjects& objects, std::string& error) {
	// 今回の削除処理の結果だけを返す。
	error.clear();

	// 親子の重複を除いた削除ルートを求める。
	const std::vector<WorldObjectId> roots = GetRoots(objects);

	// 対象がなければ配置を変更しない。
	if (roots.empty()) {
		return false;
	}

	// 元の配置から削除対象の子孫を集める。
	std::vector<WorldObjectData> original = objects.CaptureObjects();

	const std::unordered_set<WorldObjectId> included = CollectSubtrees(original, roots);

	// 削除対象以外を新しい配置へ残す。
	std::vector<WorldObjectData> remaining;

	remaining.reserve(original.size());

	for (WorldObjectData& object : original) {
		if (!included.contains(object.id)) {
			remaining.push_back(std::move(object));
		}
	}

	// 検証に失敗した場合は元の配置を維持する。
	if (!objects.ReplaceObjects(std::move(remaining), error)) {
		return false;
	}

	// 削除済みのIDを選択状態に残さない。
	Clear();

	return true;
}
