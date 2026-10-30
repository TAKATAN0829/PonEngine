#pragma once

#include "IEditableScene.h"

// 発生設定とプレビュー操作を管理する。粒子の実行データはGameSceneが所有する。
class ParticleEditorPanel {
public:

	//=============================================================================================//
	// 初期化処理

	// 共通プリセットの保存基準と、パネル操作の状態を初期化する。
	void Initialize(IEditableScene& scene);

	//=============================================================================================//
	// 更新処理

	// 選択IDから編集対象と注視位置を取得する。借用参照は持ち越さない。
	void Update(IEditableScene& scene, bool gamePlaying, SceneObjectId selectedId);

	//=============================================================================================//
	// 描画処理

	void Draw(IEditableScene& scene, bool gamePlaying, SceneObjectId selectedId, bool* open = nullptr);
	// パネルを閉じたときは編集プレビューだけを終了する。
	void StopPreview(IEditableScene& scene, bool gamePlaying);

	//=============================================================================================//
	// Getter / 設定・注視位置

	bool IsDirty(IEditableScene& scene) const;
	Vector3 GetFocusPosition() const { return focusPosition_; }

	//=============================================================================================//
	// メンバ関数 / 保存・注視要求

	bool Save(IEditableScene& scene); // 共通プリセットの保存。
	bool ConsumeFocusRequest(); // 一度だけ注視要求を取り出す。

private:

	//=============================================================================================//
	// 型 / プリセット操作

	enum class PresetAction { None, Load, Defaults };

	//=============================================================================================//
	// Getter / 編集設定

	// 返すポインタはこのフレームだけ借用する。
	ParticleSettings* GetEditingSettings(IEditableScene& scene) const;
	TransformData* GetEditingTransform(IEditableScene& scene) const;

	//=============================================================================================//
	// メンバ関数 / プリセット反映

	void ApplyPendingAction(IEditableScene& scene);
	bool SaveEditingPreset(IEditableScene& scene);

private:

	//=============================================================================================//
	// メンバ変数 / 編集対象

	IEditableScene* scene_ = nullptr; // シーン切替検出用。所有権は持たない。
	SceneObjectId targetId_ = kInvalidSceneObjectId; // 0なら共通プリセット。
	Vector3 focusPosition_{}; // Sceneカメラへ渡す発生位置。

	//=============================================================================================//
	// メンバ変数 / 保存・プレビュー

	ParticleSettings savedSettings_; // 共通プリセットの保存済み設定。
	ParticlePreviewState preview_; // プレビューの再生・停止・対象ID。
	std::string status_; // プリセット操作の結果。

	//=============================================================================================//
	// メンバ変数 / 操作要求

	PresetAction pendingAction_ = PresetAction::None;
	bool openPrompt_ = false; // 変更破棄の確認が開いているか。
	bool focusRequested_ = false; // Scene側が消費する注視要求。
};
