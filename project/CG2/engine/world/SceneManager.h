#pragma once

#include "IScene.h"
#include <memory>

// シーンの寿命とフレーム境界での切替を管理する。描画中のシーンは破棄しない。
class SceneManager {
public:

	//=============================================================================================//
	// 初期化処理

	// firstSceneの所有権を引き取る。描画機能はEngineから借用する。
	void Initialize(const SceneContext& context, IScene* firstScene);

	//=============================================================================================//
	// 更新処理

	void Update(Input* input, bool shouldSimulate = true);

	// PostDrawのGPU完了後に呼ぶ。場面が変わった場合だけtrueを返す。
	bool ApplyPendingScene();

	//=============================================================================================//
	// 描画処理

	void Draw();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Getter / 現在の場面

	IEditableWorld* GetEditableWorld(); // 現在の場面が公開する編集窓口を借用する。

	const char* GetSceneName() const;

	std::string GetInitialAssetPath() const;

	//=============================================================================================//
	// Setter / 再生状態

	// 再生中に遷移した場合、停止後のフレーム境界で元の編集シーンへ戻す。
	void SetPlayState(bool isPlaying);

private:

	//=============================================================================================//
	// 終了・所有権の解放

	static void ReleaseScene(std::unique_ptr<IScene>& scene);

	//=============================================================================================//
	// 場面と描画機能

	SceneContext context_; // Engineが所有する機能。すべてのシーンより長く生存する。

	std::unique_ptr<IScene> currentScene_;
	std::unique_ptr<IScene> pendingScene_; // まだInitializeしていない遷移先。
	std::unique_ptr<IScene> editScene_; // 最初の遷移時だけ、元の編集シーンを生存させて退避する。

	bool isPlaying_ = false;
	bool shouldRestoreEditScene_ = false;
};
