#pragma once

#include "IEditableWorld.h"

// 発生設定とプレビュー操作を管理する。粒子の実行データはWorldSceneが所有する。
class ParticleEditorPanel {
public:

	//=============================================================================================//
	// 初期化処理

	// 共通プリセットの保存基準と、パネル操作の状態を初期化する。
	void Initialize(IEditableWorld& scene);

	//=============================================================================================//
	// 更新処理

	// 選択IDから編集対象と注視位置を取得する。借用参照は持ち越さない。
	void Update(IEditableWorld& scene, bool gamePlaying, WorldObjectId selectedId);

	//=============================================================================================//
	// 描画処理

	void Draw(IEditableWorld& scene, bool gamePlaying, WorldObjectId selectedId, bool* open = nullptr);

	// パネルを閉じたときは編集プレビューだけを終了する。
	void StopPreview(IEditableWorld& scene, bool gamePlaying);

	//=============================================================================================//
	// Getter / 設定・注視位置

	bool IsDirty(IEditableWorld& scene) const;

	Vector3 GetFocusPosition() const { return focusPosition_; }

	//=============================================================================================//
	// Asset参照

	// 保存基準も更新し、名前変更だけで未保存扱いにしない。
	void RemapAssetPaths(const AssetPathChange& change);

	//=============================================================================================//
	// 保存・注視要求

	bool Save(IEditableWorld& scene); // 共通プリセットの保存。

	bool ConsumeFocusRequest(); // 一度だけ注視要求を取り出す。

private:

	//=============================================================================================//
	// 型 / プリセット操作

	enum class PresetAction { None, Load, Defaults, Fire, Smoke, Sparks };

	//=============================================================================================//
	// Getter / 編集設定

	// 返すポインタはこのフレームだけ借用する。
	ParticleSettings* GetEditingSettings(IEditableWorld& scene) const;

	TransformData* GetEditingTransform(IEditableWorld& scene) const;

	//=============================================================================================//
	// プリセット反映

	void ApplyPendingAction(IEditableWorld& scene);

	bool SaveEditingPreset(IEditableWorld& scene);

private:

	//=============================================================================================//
	// 編集対象

	IEditableWorld* scene_ = nullptr; // シーン切替検出用。所有権は持たない。

	WorldObjectId targetId_ = kInvalidWorldObjectId; // 0なら共通プリセット。

	Vector3 focusPosition_{}; // Sceneカメラへ渡す発生位置。

	//=============================================================================================//
	// 保存・プレビュー

	ParticleSettings savedSettings_; // 共通プリセットの保存済み設定。

	ParticlePreviewState preview_; // プレビューの再生・停止・対象ID。

	std::string status_; // プリセット操作の結果。

	//=============================================================================================//
	// 操作要求

	PresetAction pendingAction_ = PresetAction::None;

	bool openPrompt_ = false; // 変更破棄の確認が開いているか。
	bool focusRequested_ = false; // Scene側が消費する注視要求。
};
