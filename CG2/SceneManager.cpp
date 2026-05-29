#include "SceneManager.h"

//=================================================================================================//
// 初期化処理

void SceneManager::Initialize(int32_t clientWidth, int32_t clientHeight) {


	//=============================================================================================//
	// ClientWidth保存

	clientWidth_ = clientWidth;

	//=============================================================================================//
	// ClientHeight保存

	clientHeight_ = clientHeight;

	//=============================================================================================//
	// SceneContext設定

	sceneContext_.clientWidth = clientWidth_;

	sceneContext_.clientHeight = clientHeight_;

	//=============================================================================================//
	// TitleScene生成

	currentScene_ = new TitleScene();

	currentScene_->SetContext(sceneContext_);

	currentScene_->Initialize();
}

//=================================================================================================//
// 更新処理

void SceneManager::Update(Input* input) {

	//=============================================================================================//
	// CurrentScene更新

	currentScene_->Update(input);

	//=============================================================================================//
	// シーン切り替え

	if (currentScene_->IsFinished()) {

		//=========================================================================================//
		// CurrentScene解放

		currentScene_->Finalize();

		delete currentScene_;

		currentScene_ = nullptr;

		//=========================================================================================//
		// GameScene生成

		GameScene* gameScene = new GameScene();

		gameScene->SetContext(sceneContext_);

		gameScene->Initialize();

		currentScene_ = gameScene;
	}
}

//=================================================================================================//
// 描画処理

void SceneManager::Draw() {

	//=============================================================================================//
	// CurrentScene描画

	currentScene_->Draw();
}

//=================================================================================================//
// 終了処理

void SceneManager::Finalize() {

	//=============================================================================================//
	// CurrentScene解放

	if (currentScene_ != nullptr) {

		currentScene_->Finalize();

		delete currentScene_;

		currentScene_ = nullptr;
	}
}