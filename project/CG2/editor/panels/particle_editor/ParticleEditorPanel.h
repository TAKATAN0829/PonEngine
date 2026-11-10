#pragma once

#include "../EditorPanel.h"
#include "IEditableWorld.h"

// 発生設定とプレビュー操作を管理する。粒子の実行データはWorldSceneが所有する。
class ParticleEditorPanel : public EditorPanel {
public:
	ParticleEditorPanel() : EditorPanel("Particle Editor") {}

	//=============================================================================================//
	// 初期化処理

	// 共通プリセットの保存基準と、パネル操作の状態を初期化する。
	void Initialize(IWorldObjects& objects, IParticlePreviewWorld& particles);

	// 共通プリセットの保存基準は保持し、Scene固有の対象と操作だけを解除する。
	void ResetScenePreview(IWorldObjects& objects, IParticlePreviewWorld& particles);

	//=============================================================================================//
	// 更新処理

	// 選択IDから編集対象と注視位置を取得する。借用参照は持ち越さない。
	void Update(IWorldObjects& objects, IParticlePreviewWorld& particles, bool isGamePlaying, WorldObjectId selectedId);

	//=============================================================================================//
	// 描画処理

	void Draw(IWorldObjects& objects, IParticlePreviewWorld& particles, ITextureAssetAccess& textures,
		bool isGamePlaying, WorldObjectId selectedId, bool* isOpen = nullptr);

	// パネルを閉じたときは編集プレビューだけを終了する。
	void StopPreview(IParticlePreviewWorld& particles, bool isGamePlaying);

	//=============================================================================================//
	// Getter / 設定・注視位置

	bool IsDirty(IWorldObjects& objects) const;

	Vector3 GetFocusPosition() const;

	//=============================================================================================//
	// Asset参照

	// 保存基準も更新し、名前変更だけで未保存扱いにしない。
	void RemapAssetPaths(const AssetPathChange& change);

	//=============================================================================================//
	// 保存・注視要求

	bool Save(IParticlePreviewWorld& particles); // 共通プリセットの保存。

	bool ConsumeFocusRequest(); // 一度だけ注視要求を取り出す。

private:

	//=============================================================================================//
	// 型 / プリセット操作

	enum class PresetAction {
		None, 
		Load,
		Defaults,
		Fire, 
		Smoke, 
		Sparks 
	};

	ParticleEffectDocument* GetEditingDocument(IWorldObjects& objects);

	//=============================================================================================//
	// プリセット反映

	void ApplyPendingAction(IWorldObjects& objects, IParticlePreviewWorld& particles);

	bool SaveEditingPreset(IWorldObjects& objects, IParticlePreviewWorld& particles);

private:

	//=============================================================================================//
	// 編集対象

	IWorldObjects* sceneObjects_ = nullptr; // シーン切替検出用。所有権は持たない。

	WorldObjectId targetId_ = kInvalidWorldObjectId; // 0なら共通プリセット。
	uint64_t selectedNodeId_ = 0;
	ParticleEffectDocument commonDocument_; // 共通プリセットの編集文書。現状はEmitter 1個。

	Vector3 focusPosition_{}; // Sceneカメラへ渡す発生位置。

	//=============================================================================================//
	// 保存・プレビュー

	ParticleSettings savedSettings_; // 共通プリセットの保存済み設定。
	ParticleEffectDocument savedDocument_; // 共通プリセットの保存済み文書。

	ParticlePreviewState preview_; // プレビューの再生・停止・対象ID。

	std::string status_; // プリセット操作の結果。

	//=============================================================================================//
	// 操作要求

	PresetAction pendingAction_ = PresetAction::None;

	bool shouldOpenPrompt_ = false; // 変更破棄の確認が開いているか。
	bool isFocusRequested_ = false; // Scene側が消費する注視要求。
};
