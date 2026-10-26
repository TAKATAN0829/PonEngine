#include "Engine.h"

// 自作
#include "TextureManager.h"
#include "ModelManager.h"

//=================================================================================================//
// 初期化処理

void Engine::Initialize(IScene* firstScene) {

	//=============================================================================================//
	// Window初期化

	winApp_ = new WinApp();

	winApp_->Initialize();

	//=============================================================================================//
	// DirectXCommon初期化

	dxCommon_ = new DirectXCommon();

	dxCommon_->Initialize(winApp_);

	//=============================================================================================//
	// SrvManager初期化

	srvManager_ = new SrvManager();

	srvManager_->Initialize(dxCommon_);

	//=============================================================================================//
	// Input初期化

	input_ = new Input();

	input_->Initialize(winApp_);

	//=============================================================================================//
	// AudioSystem初期化

	audioSystem_ = new AudioSystem();

	audioSystem_->Initialize();

	//=============================================================================================//
	// TextureManager初期化

	TextureManager::GetInstance()->Initialize(dxCommon_, srvManager_);

	TextureManager::GetInstance()->Load("resources/white1x1.png");

	//=============================================================================================//
	// ModelManager初期化

	ModelManager::GetInstance()->Initialize(dxCommon_, srvManager_);

	//=============================================================================================//
	// ShaderCompiler初期化

	shaderCompiler_ = new ShaderCompiler();

	shaderCompiler_->Initialize();

	//=============================================================================================//
	// Renderer初期化

	object3dCommon_ = new Object3dCommon();

	object3dCommon_->Initialize(dxCommon_, srvManager_, shaderCompiler_);

	particleRenderer_ = new ParticleRenderer();

	particleRenderer_->Initialize(dxCommon_, srvManager_, shaderCompiler_);

	//=============================================================================================//
	// SpriteCommon初期化

	spriteCommon_ = new SpriteCommon();

	spriteCommon_->Initialize(dxCommon_, srvManager_, shaderCompiler_);

	//=============================================================================================//
	// ImGuiSystem初期化

	imGuiSystem_ = new ImGuiSystem();

	imGuiSystem_->Initialize(winApp_, dxCommon_, srvManager_);

	//=============================================================================================//
	// SceneManager初期化

	SceneContext sceneContext{};

	sceneContext.object3dCommon = object3dCommon_;

	sceneContext.spriteCommon = spriteCommon_;

	sceneContext.particleRenderer = particleRenderer_;

	sceneManager_ = new SceneManager();

	sceneManager_->Initialize(sceneContext, firstScene);
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
		// SRV描画前処理

		srvManager_->PreDraw();

		//=========================================================================================//
		// ImGui開始

		imGuiSystem_->Begin();

		//=========================================================================================//
		// シーン更新

		sceneManager_->Update(input_);

		//=========================================================================================//
		// ImGui受付終了

		imGuiSystem_->End();

		//=========================================================================================//
		// シーン描画

		sceneManager_->Draw();

		//=========================================================================================//
		// ImGui描画

		imGuiSystem_->Draw();

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

	delete sceneManager_;

	//=============================================================================================//
	// ImGuiSystem終了

	imGuiSystem_->Finalize();

	delete imGuiSystem_;

	//=============================================================================================//
	// SpriteCommon解放

	spriteCommon_->Finalize();

	delete spriteCommon_;

	//=============================================================================================//
	// Renderer解放

	particleRenderer_->Finalize();

	delete particleRenderer_;

	delete object3dCommon_;

	//=============================================================================================//
	// ShaderCompiler終了

	shaderCompiler_->Finalize();

	delete shaderCompiler_;

	//=============================================================================================//
	// ModelManager終了

	ModelManager::GetInstance()->Finalize();

	//=============================================================================================//
	// TextureManager終了

	TextureManager::GetInstance()->Finalize();

	//=============================================================================================//
	// AudioSystem終了

	audioSystem_->Finalize();

	delete audioSystem_;

	//=============================================================================================//
	// Input終了

	input_->Finalize();

	delete input_;

	//=============================================================================================//
	// SrvManager解放

	delete srvManager_;

	//=============================================================================================//
	// DirectXCommon終了

	dxCommon_->Finalize();

	delete dxCommon_;

	//=============================================================================================//
	// Window終了

	winApp_->Finalize();

	delete winApp_;
}
