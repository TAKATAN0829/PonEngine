#pragma once

#include "AssetFileOperations.h"

class EditorSession;
class ParticleEditorPanel;

// Projectの確認画面を担当する。ファイルの検証・更新はAssetFileOperationsへ委譲する。
class AssetActionDialog {
public:
	//=============================================================================================//
	// 初期化処理

	void Initialize(const std::filesystem::path& root);

	//=============================================================================================//
	// 描画処理

	void DrawMenu(const std::filesystem::path& source, bool directory, bool allowChanges);

	void DrawCreateMenu(const std::filesystem::path& directory, bool allowChanges);

	void AcceptMoveDrop(const std::filesystem::path& directory, bool allowChanges);

	void Draw(IEditableWorld& scene, EditorSession& history, ParticleEditorPanel& particles);

	//=============================================================================================//
	// Getter / 操作結果

	const std::string& GetStatus() const { return status_; }

	bool ConsumeRefresh();

	bool ConsumePathChange(AssetPathChange& change);

private:
	//=============================================================================================//
	// 型 / 操作内容

	enum class Action { None, Rename, Move, References, Delete, CreateFolder };

	//=============================================================================================//
	// 操作の準備・確定

	void Request(Action action, const std::filesystem::path& source);

	std::vector<AssetReference> CollectReferences(IEditableWorld& scene, const EditorSession& history) const;

	bool Execute(IEditableWorld& scene, EditorSession& history, ParticleEditorPanel& particles);

	//=============================================================================================//
	// 確認待ちの操作

	AssetFileOperations operations_;

	std::filesystem::path root_;
	std::filesystem::path source_;

	Action action_ = Action::None;

	char input_[256]{};

	std::string status_;

	std::vector<AssetReference> references_;

	AssetPathChange pathChange_;

	bool openRequested_ = false;
	bool inspected_ = false;
	bool refreshRequested_ = false;
	bool pathChanged_ = false;
};
