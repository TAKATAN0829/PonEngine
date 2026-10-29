#pragma once

// 自作
#include "IScene.h"
#include "Input.h"

class SceneManager {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(const SceneContext& context, IScene* firstScene);

	//=============================================================================================//
	// 更新処理

	void Update(Input* input, bool simulate = true);
	void SetPlayState(bool playing);

	//=============================================================================================//
	// 描画処理

	void Draw();

	// 現在のシーンが公開する編集窓口を借用する。シーンの所有権は渡さない。
	IEditableScene* GetEditableScene() {
		if (currentScene_ == nullptr) {
			return nullptr;
		}
		return currentScene_->GetEditableScene();
	}

	//=============================================================================================//
	// 終了処理

	void Finalize();

private:

	//=============================================================================================//
	// CurrentScene

	IScene* currentScene_ = nullptr;
};
