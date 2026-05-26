#include "SceneManager.h"

//=================================================================================================//
// 初期化処理

void SceneManager::Initialize (
	ID3D12Device* device,
	ResourceManager* resourceManager,
	int32_t clientWidth,
	int32_t clientHeight) {

	device_ = device;

	resourceManager_ = resourceManager;

	clientWidth_ = clientWidth;

	clientHeight_ =	clientHeight;

	//=============================================================================================//
	// TitleScene生成

	currentScene_ =	new TitleScene ();

	currentScene_->Initialize ();
}

//=================================================================================================//
// 更新処理

void SceneManager::Update (Input* input) {

	currentScene_->Update (input);

	if (currentScene_->IsFinished ()) {

		currentScene_->Finalize ();

		delete currentScene_;

		//=========================================================================================//
		// GameScene生成

		GameScene* gameScene = new GameScene ();

		gameScene->SetDevice (device_);

		gameScene->SetResourceManager (resourceManager_);

		gameScene->SetWindowSize (clientWidth_,	clientHeight_);

		gameScene->Initialize ();

		currentScene_ =	gameScene;
	}
}

//=================================================================================================//
// 描画処理

void SceneManager::Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	currentScene_->Draw (commandList, graphicsSystem);
}

//=================================================================================================//
// 終了処理

void SceneManager::Finalize () {

	if (currentScene_ != nullptr) {

		currentScene_->Finalize ();

		delete currentScene_;

		currentScene_ = nullptr;
	}
}