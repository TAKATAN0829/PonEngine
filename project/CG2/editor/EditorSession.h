#pragma once

#include "IEditableScene.h"

// 履歴と再生前の設定を所有する。描画はEditorが担当し、このクラスは編集データを管理する。
class EditorSession {
public:

	//=============================================================================================//
	// 初期化処理

	// 読込後の設定を履歴と保存済み状態の基準へ戻す。
	void Initialize(IEditableScene& scene);

	//=============================================================================================//
	// 更新処理

	// ドラッグ・文字入力が終わった時点で履歴を一件記録する。
	void Update(IEditableScene& scene, bool interactionActive);

	//=============================================================================================//
	// Getter / 履歴・保存状態

	bool CanUndo() const;
	bool CanRedo() const;
	bool IsDirty(const IEditableScene& scene) const;

	//=============================================================================================//
	// Getter / 再生状態

	bool IsPlaying() const { return playing_; }
	bool IsPaused() const { return paused_; }
	bool ShouldSimulate(); // 一回分のStep要求はここで消費する。

	//=============================================================================================//
	// Setter / 保存基準

	void MarkSaved(const IEditableScene& scene);

	//=============================================================================================//
	// メンバ関数 / 履歴操作

	// 復元が失敗した場合は現在の設定と履歴位置を保持する。
	bool Undo(IEditableScene& scene, std::string& error);
	bool Redo(IEditableScene& scene, std::string& error);

	//=============================================================================================//
	// メンバ関数 / 再生操作

	bool BeginPlay(IEditableScene& scene, std::string& error);
	bool Stop(IEditableScene& scene, std::string& error);
	void TogglePause();
	void RequestStep();

private:

	//=============================================================================================//
	// メンバ関数 / 設定比較

	// 読取専用参照で比較し、変更がある場合にだけ履歴用のコピーを作る。
	static bool MatchesSnapshot(const IEditableScene& scene, const std::vector<SceneObjectData>& snapshot);
	static bool AreObjectsEqual(const SceneObjectData& firstObject, const SceneObjectData& secondObject);
	static bool AreScenesEqual(const std::vector<SceneObjectData>& firstScene, const std::vector<SceneObjectData>& secondScene);

private:

	//=============================================================================================//
	// 定数 / 履歴上限

	static constexpr size_t kMaxHistoryStates = 64;

	//=============================================================================================//
	// メンバ変数 / 履歴

	std::vector<std::vector<SceneObjectData>> history_; // GPUリソースを含まない設定。
	size_t historyIndex_ = 0; // 現在表示している履歴の位置。
	std::vector<SceneObjectData> savedState_; // 保存成功時の比較基準。

	//=============================================================================================//
	// メンバ変数 / 再生

	std::vector<SceneObjectData> playSnapshot_; // 停止時に戻す編集状態。
	bool playing_ = false;
	bool paused_ = false;
	bool stepRequested_ = false; // 次の更新で一度だけ消費する。
};
