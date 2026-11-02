#pragma once

#include "IEditableWorld.h"

// 履歴と再生前の設定を所有する。描画はEditorが担当し、このクラスは編集データを管理する。
class EditorSession {
public:

	//=============================================================================================//
	// 初期化処理

	// 読込後の設定を履歴と保存済み状態の基準へ戻す。
	void Initialize(IEditableWorld& scene);

	//=============================================================================================//
	// 更新処理

	// ドラッグ・文字入力が終わった時点で履歴を一件記録する。
	void Update(IEditableWorld& scene, bool interactionActive);

	//=============================================================================================//
	// Getter / 履歴・保存状態

	bool CanUndo() const;
	bool CanRedo() const;
	bool IsDirty(const IEditableWorld& scene) const;

	//=============================================================================================//
	// Getter / 再生状態

	bool IsPlaying() const { return playing_; }
	bool IsPaused() const { return paused_; }
	bool ShouldSimulate(); // 一回分のStep要求はここで消費する。

	//=============================================================================================//
	// Setter / 保存基準

	void MarkSaved(const IEditableWorld& scene);

	//=============================================================================================//
	// メンバ関数 / 履歴操作

	// 復元が失敗した場合は現在の設定と履歴位置を保持する。
	bool Undo(IEditableWorld& scene, std::string& error);
	bool Redo(IEditableWorld& scene, std::string& error);

	//=============================================================================================//
	// メンバ関数 / 再生操作

	bool BeginPlay(IEditableWorld& scene, std::string& error);
	bool Stop(IEditableWorld& scene, std::string& error);
	void TogglePause();
	void RequestStep();

private:

	//=============================================================================================//
	// メンバ関数 / 設定比較

	// 読取専用参照で比較し、変更がある場合にだけ履歴用のコピーを作る。
	static bool MatchesSnapshot(const IEditableWorld& scene, const std::vector<WorldObjectData>& snapshot);
	static bool AreObjectsEqual(const WorldObjectData& firstObject, const WorldObjectData& secondObject);
	static bool AreScenesEqual(const std::vector<WorldObjectData>& firstScene, const std::vector<WorldObjectData>& secondScene);

private:

	//=============================================================================================//
	// 定数 / 履歴上限

	static constexpr size_t kMaxHistoryStates = 64;

	//=============================================================================================//
	// メンバ変数 / 履歴

	std::vector<std::vector<WorldObjectData>> history_; // GPUリソースを含まない設定。
	size_t historyIndex_ = 0; // 現在表示している履歴の位置。
	std::vector<WorldObjectData> savedState_; // 保存成功時の比較基準。

	//=============================================================================================//
	// メンバ変数 / 再生

	std::vector<WorldObjectData> playSnapshot_; // 停止時に戻す編集状態。
	bool playing_ = false;
	bool paused_ = false;
	bool stepRequested_ = false; // 次の更新で一度だけ消費する。
};
