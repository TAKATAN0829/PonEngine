#pragma once

// 自作
#include "EditorSelection.h"
#include "EditorSession.h"
#include "panels/scene_view/SceneViewPanel.h"
#include "panels/particle_editor/ParticleEditorPanel.h"
#include "panels/project/ProjectPanel.h"
#include "panels/console/ConsolePanel.h"
#include "panels/performance/PerformancePanel.h"
#include "EditorLayout.h"

// C++
#include <string>

// パネルの描画と編集状態の更新をまとめ、保存・再生・終了確認を管理する。
class Editor {
public:

	//=============================================================================================//
	// 初期化処理

	// 対象シーンに合わせて選択・履歴・確認待ちの状態を初期化する。
	void Initialize(IEditableWorld* scene);

	//=============================================================================================//
	// 更新処理

	// パネル操作後の履歴確定、ショートカット、ログ通知を処理する。
	void Update(IEditableWorld* scene);

	//=============================================================================================//
	// 描画処理

	// メニューと各パネルを描く。GPUへの反映より前に呼び出す。
	void Draw(IEditableWorld* scene);

	//=============================================================================================//
	// Getter / 再生・終了状態

	bool IsPlaying() const { return session_.IsPlaying(); }
	bool ShouldSimulate(); // Step要求を一度だけ消費して更新可否を返す。
	bool ShouldExit() const { return exitApproved_; }

	//=============================================================================================//
	// Getter / Scene表示サイズ

	uint32_t GetSceneWidth() const { return sceneView_.GetWidth(); }
	uint32_t GetSceneHeight() const { return sceneView_.GetHeight(); }

	//=============================================================================================//
	// Setter / Scene画像

	// 描画先が所有するSRVハンドルを借用する。
	void SetSceneTexture(uint64_t textureHandle) { sceneView_.SetTexture(textureHandle); }

	//=============================================================================================//
	// 終了要求

	// 未保存の設定があれば確認を保留し、許可後に終了状態を公開する。
	void RequestExit(IEditableWorld* scene);

private:

	//=============================================================================================//
	// 型 / シーン操作

	enum class SceneAction { None, NewScene, LoadScene, Exit };

	//=============================================================================================//
	// シーン保存・切替

	bool SaveScene(IEditableWorld& scene);
	void RequestSceneAction(SceneAction action, IEditableWorld& scene);
	void PerformSceneAction(IEditableWorld& scene);
	bool NeedsSaveConfirmation(SceneAction action, IEditableWorld& scene) const;

	//=============================================================================================//
	// パネル描画

	void DrawToolbar(IEditableWorld& scene);
	void DrawDiscardPrompt(IEditableWorld& scene);

	//=============================================================================================//
	// 入力・再生

	void HandleShortcuts(IEditableWorld& scene);
	void TogglePlay(IEditableWorld& scene);

private:

	//=============================================================================================//
	// メンバ変数 / 編集対象

	// SceneManagerが所有するシーン。切替検出用に借用し、破棄しない。
	IEditableWorld* scene_ = nullptr;
	EditorSelection selection_;
	EditorSession session_;

	//=============================================================================================//
	// メンバ変数 / パネル

	SceneViewPanel sceneView_; // 画像上の選択と移動操作。
	ParticleEditorPanel particleEditor_; // Emitter設定とプレビュー。
	ProjectPanel projectPanel_; // フォルダ・Assetの選択とプレビュー。
	ConsolePanel consolePanel_; // ログの検索・表示。
	PerformancePanel performancePanel_; // 計測履歴の選択とCSV出力の表示。
	EditorToolVisibility toolVisibility_; // 補助画面は初期状態ではすべて閉じる。
	EditorLayout layout_; // メニュー・初期ドッキング・通知欄。

	//=============================================================================================//
	// メンバ変数 / 通知

	std::string fileStatus_; // 保存・読込の結果。
	std::string loggedStatus_; // 同じ結果をログへ重複出力しないための比較値。

	//=============================================================================================//
	// メンバ変数 / 保存確認・終了

	SceneAction pendingAction_ = SceneAction::None; // 保存確認後に実行する操作。
	bool openDiscardPrompt_ = false; // 次の描画で確認ダイアログを開く。
	bool exitApproved_ = false; // Engineが終了できる状態。
};
