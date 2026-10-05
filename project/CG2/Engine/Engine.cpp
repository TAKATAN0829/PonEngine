#include "Engine.h"

//=================================================================================================//
// 初期化処理

void Engine::Initialize() {

	//=============================================================================================//
	// Window初期化

	winApp_ = new WinApp();

	winApp_->Initialize();

	//=============================================================================================//
	// DirectXCommon初期化

	dxCommon_ = new DirectXCommon();

	dxCommon_->Initialize(winApp_);

	//=============================================================================================//
	// Input初期化

	input_ = new Input();

	input_->Initialize(winApp_);

	//=============================================================================================//
	// AudioSystem初期化

	audioSystem_ = new AudioSystem();

	audioSystem_->Initialize();

	//=============================================================================================//
	// GraphicsSystem初期化

	graphicsSystem_ = new GraphicsSystem();

	GraphicsSystem::SetGraphicsSystem(graphicsSystem_);

	GraphicsSystem::SetDevice(dxCommon_->GetDevice());

	GraphicsSystem::SetCommandList (dxCommon_->GetCommandList ());

	graphicsSystem_->Initialize(dxCommon_->GetDevice());
	
	ResourceManager::SetInstance(graphicsSystem_->GetResourceManager());

	graphicsSystem_->CreateTexture(
		dxCommon_->GetDevice(),
		dxCommon_->GetCommandList(),
		"./Resources/white1x1.png");

	//=============================================================================================//
	// ImGuiSystem初期化

	imGuiSystem_ = new ImGuiSystem();

	imGuiSystem_->Initialize(
		winApp_->GetHwnd(),
		dxCommon_->GetDevice(),
		graphicsSystem_->GetSRVDescriptorHeap(),
		2,
		1);

	//=============================================================================================//
	// SceneManager初期化

	sceneManager_ = new SceneManager();

	sceneManager_->Initialize();
}

//=================================================================================================//
// 実行処理

void Engine::Run() {

	while (true) {

		//=========================================================================================//
		// Windowsのメッセージ処理

		if (winApp_->ProcessMessage()) {
			break;
		}

		//=========================================================================================//
		// Window更新

		winApp_->Update();

		//=========================================================================================//
		// Input更新

		input_->Update();

		//=========================================================================================//
		// 描画前処理

		dxCommon_->PreDraw();

		//=========================================================================================//
		// コマンドリスト取得

		GraphicsSystem::SetCommandList(dxCommon_->GetCommandList());

		//=========================================================================================//
		// ImGui開始

		imGuiSystem_->Begin();

		//=========================================================================================//
		// シーン更新

		sceneManager_->Update(input_);

		//=========================================================================================//
		// シーン描画

		sceneManager_->Draw();

		//=========================================================================================//
		// ImGui描画

		imGuiSystem_->Draw(dxCommon_->GetCommandList());

		//=========================================================================================//
		// 描画後処理

		dxCommon_->PostDraw();
	}
}

//=================================================================================================//
// 終了処理

void Engine::Finalize() {

	//=============================================================================================//
	// SceneManager終了

	sceneManager_->Finalize();

	//=============================================================================================//
	// ImGuiSystem終了

	imGuiSystem_->Finalize();

	//=============================================================================================//
	// 各System終了

	graphicsSystem_->Finalize();

	audioSystem_->Finalize();

	input_->Finalize();

	dxCommon_->Finalize();

	winApp_->Finalize();

	//=============================================================================================//
	// 解放

	delete sceneManager_;

	delete imGuiSystem_;

	delete graphicsSystem_;

	delete audioSystem_;

	delete input_;

	delete dxCommon_;

	delete winApp_;
}