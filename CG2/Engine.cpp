#include "Engine.h"

//=================================================================================================//
// 初期化処理

void Engine::Initialize () {

	//=============================================================================================//
	// Window初期化

	winApp_ = new WinApp ();

	winApp_->Initialize ();

	//=============================================================================================//
	// DirectXCommon初期化

	dxCommon_ = new DirectXCommon ();

	dxCommon_->Initialize (
		winApp_->GetHwnd (),
		winApp_->GetClientWidth (),
		winApp_->GetClientHeight ());

	//=============================================================================================//
	// Input初期化

	input_ = new Input ();

	input_->Initialize (GetModuleHandle (nullptr), winApp_->GetHwnd ());

	//=============================================================================================//
	// AudioSystem初期化

	audioSystem_ = new AudioSystem ();

	audioSystem_->Initialize ();

	//=============================================================================================//
	// GraphicsSystem初期化

	graphicsSystem_ = new GraphicsSystem ();

	graphicsSystem_->Initialize (dxCommon_->GetDevice ());

	graphicsSystem_->CreateGraphicsPipeline (dxCommon_->GetDevice ());
	graphicsSystem_->CreateSpritePipeline(dxCommon_->GetDevice());

	graphicsSystem_->CreateTexture (
		dxCommon_->GetDevice (),
		dxCommon_->GetCommandList (),
		"./Resources/uvChecker.png");

	//=============================================================================================//
	// SceneManager初期化

	sceneManager_ = new SceneManager ();

	sceneManager_->Initialize (
		dxCommon_->GetDevice (),
		graphicsSystem_->GetResourceManager (),
		winApp_->GetClientWidth (),
		winApp_->GetClientHeight ());
}

//=================================================================================================//
// 実行処理

void Engine::Run () {

	while (winApp_->ProcessMessage ()) {

		//=========================================================================================//
		// Input更新

		input_->Update ();

		//=========================================================================================//
		// Frame開始

		dxCommon_->BeginFrame ();

		//=========================================================================================//
		// シーン更新・描画

		sceneManager_->Update (input_);

		sceneManager_->Draw (dxCommon_->GetCommandList (),graphicsSystem_);

		//=========================================================================================//
		// Frame終了

		dxCommon_->EndFrame ();
	}
}

//=================================================================================================//
// 終了処理

void Engine::Finalize () {

	sceneManager_->Finalize ();
	graphicsSystem_->Finalize ();
	audioSystem_->Finalize ();
	input_->Finalize ();
	dxCommon_->Finalize ();
	winApp_->Finalize ();

	delete sceneManager_;
	delete graphicsSystem_;
	delete audioSystem_;
	delete input_;
	delete dxCommon_;
	delete winApp_;
}