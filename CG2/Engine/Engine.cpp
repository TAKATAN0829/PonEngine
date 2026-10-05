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

	dxCommon_->Initialize(
		winApp_->GetHwnd(),
		WinApp::kClientWidth,
		WinApp::kClientHeight);

	//=============================================================================================//
	// Input初期化

	input_ = new Input();

	input_->Initialize(winApp_->GetHInstance(), winApp_->GetHwnd());

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

	graphicsSystem_->CreateGraphicsPipeline(dxCommon_->GetDevice());

	graphicsSystem_->CreateSpritePipeline(dxCommon_->GetDevice());

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

	sceneManager_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);
}

//=================================================================================================//
// 実行処理

void Engine::Run() {

	while (true) {

		//=========================================================================================//
		// Window更新

		winApp_->Update();

		// 終了リクエストが来たらループを抜ける
		if (winApp_->IsEndRequest()) {
			break;
		}

		//=========================================================================================//
		// Input更新

		input_->Update();

		//=========================================================================================//
		// Frame開始

		dxCommon_->BeginFrame();

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
		// Frame終了

		dxCommon_->EndFrame();
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