#pragma once

// 自作
#include "../state/EditorSelection.h"
#include "../state/EditorSession.h"
#include "../panels/scene_view/SceneViewPanel.h"
#include "../panels/particle_editor/ParticleEditorPanel.h"
#include "../panels/project/ProjectPanel.h"
#include "../panels/console/ConsolePanel.h"
#include "../panels/performance/PerformancePanel.h"
#include "../ui/EditorLayout.h"
#include "../scene/SceneDocument.h"
#include "../scene/SceneAssetDialog.h"

// C++
#include <string>
#include <optional>

// パネルの描画と編集状態の更新をまとめ、保存・再生・終了確認を管理する。
class Editor {
public:

	//=============================================================================================//
	// 初期化処理

	// ImGui初期化後、最初のフレームより前にEditor専用の表示設定を適用する。
	void InitializeUi();

	// 対象シーンに合わせて選択・履歴・確認待ちの状態を初期化する。
	void Initialize(IEditableWorld* scene, const std::string& initialPath = {});

	// 再生中の遷移では履歴と停止時の復元先を保持し、借用画像や選択だけを切り替える。
	void OnSceneChanged(IEditableWorld* scene, const char* sceneName, const std::string& initialPath = {});

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

	bool IsPlaying() const;

	bool ShouldSimulate(); // Step要求を一度だけ消費して更新可否を返す。

	bool ShouldExit() const;

	const std::filesystem::path& GetScenePath() const;

	//=============================================================================================//
	// Getter / Scene表示サイズ

	uint32_t GetSceneWidth() const;

	uint32_t GetSceneHeight() const;

	//=============================================================================================//
	// Setter / Scene画像

	// 描画先が所有するSRVハンドルを借用する。
	void SetSceneTexture(uint64_t textureHandle);

	//=============================================================================================//
	// 終了要求

	// 未保存の設定があれば確認を保留し、許可後に終了状態を公開する。
	void RequestExit(IEditableWorld* scene);

private:

	//=============================================================================================//
	// 型 / シーン操作

	enum class SceneAction { None, NewScene, LoadScene, Exit };

	enum class FileDialogAction { None, Open, SaveAs, SaveBeforeAction };

	//=============================================================================================//
	// シーン保存・切替

	bool SaveScene(IEditableWorld& scene);

	void OpenSceneDialog();

	void OpenSaveDialog(bool shouldSaveBeforeAction, const std::filesystem::path& suggestion = {});

	void RequestSceneAction(SceneAction action, IEditableWorld& scene, const std::filesystem::path& path = {});

	void PerformSceneAction(IEditableWorld& scene);

	void ResetSceneEditingState(IEditableWorld& scene);

	// 選択と、それに紐づくHierarchy・Inspectorの操作状態を解除する。
	void ResetPanelSelection();

	bool IsSaveConfirmationNeeded(SceneAction action, IEditableWorld& scene) const;

	// 遷移前のParticleプリセットも、元の保存基準と組にして確認・保存する。
	bool IsParticlePresetDirty(IEditableWorld& scene) const;

	bool SaveParticlePreset(IEditableWorld& scene);

	//=============================================================================================//
	// パネル描画

	void DrawToolbar(IEditableWorld& scene);

	void DrawDiscardPrompt(IEditableWorld& scene);

	void DrawSceneAssetDialog(IEditableWorld& scene);

	//=============================================================================================//
	// 入力・再生

	void HandleShortcuts(IEditableWorld& scene);

	void TogglePlay(IEditableWorld& scene);

	// 遷移中も保存・停止は、最初に再生した編集シーンを対象にする。
	IEditableWorld& GetEditWorld(IEditableWorld& scene) const;

private:

	//=============================================================================================//
	// 編集対象

	// SceneManagerが所有するシーン。
	IEditableWorld* scene_ = nullptr;
	IEditableWorld* playOrigin_ = nullptr; // SceneManagerが停止まで生存させる、再生開始時の編集対象。

	std::string sceneName_ = "GameScene"; // 現在の場面名を所有し、シーン破棄後の文字列を参照しない。

	EditorSelection selection_;

	EditorSession session_;

	//=============================================================================================//
	// Sceneファイル

	SceneDocument document_; // 再生中の場面遷移でも、編集元の保存先を保持する。

	SceneAssetDialog sceneFileDialog_;

	//=============================================================================================//
	// パネル

	SceneViewPanel sceneView_; // 画像上の選択と移動操作。

	ParticleEditorPanel particleEditor_; // Emitter設定とプレビュー。

	std::optional<ParticleEditorPanel> editParticleEditor_; // 遷移中だけ元の保存基準を保持する。GPUリソースは所有しない。

	ProjectPanel projectPanel_; // フォルダ・Assetの選択とプレビュー。

	ConsolePanel consolePanel_; // ログの検索・表示。

	PerformancePanel performancePanel_; // 計測履歴の選択とCSV出力の表示。

	EditorToolVisibility toolVisibility_{}; // 補助画面は初期状態ではすべて閉じる。

	EditorLayout layout_; // メニュー・初期ドッキング・通知欄。

	//=============================================================================================//
	// 通知

	std::string fileStatus_; // 保存・読込の結果。
	std::string loggedStatus_; // 同じ結果をログへ重複出力しないための比較値。

	//=============================================================================================//
	// 保存確認・終了

	std::filesystem::path pendingScenePath_; // 未保存確認中も開く対象を保持する。

	FileDialogAction fileDialogAction_ = FileDialogAction::None;

	SceneAction pendingAction_ = SceneAction::None; // 保存確認後に実行する操作。

	bool shouldOpenDiscardPrompt_ = false; // 次の描画で確認ダイアログを開く。
	bool isExitApproved_ = false; // Engineが終了できる状態。
};
