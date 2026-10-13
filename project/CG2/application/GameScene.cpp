#include "GameScene.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=================================================================================================//
// 初期化処理

void GameScene::Initialize(const SceneContext& context) {

	//=============================================================================================//
	// Camera初期化

	camera_ = new Camera();

	camera_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);

	cameraTransform_ = camera_->GetTransform();

	cameraTransform_.translate = { 0.f,0.f,-30.f };

	debugCamera_ = new DebugCamera();

	debugCamera_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);

	//=============================================================================================//
	// Particle初期化

	particle_ = new Particle();

	particle_->Initialize(context.particleRenderer);

	particle_->SetTexture("resources/circle.png");
}

//=================================================================================================//
// 更新処理

void GameScene::Update(Input* input) {

	// Camera更新
	UpdateCamera(input);

	// Object更新
	UpdateObject();
}

//=================================================================================================//
// 描画処理

void GameScene::Draw() {

	//=============================================================================================//
	// Particle描画

	particle_->Draw();
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize() {

	//=============================================================================================//
	// Particle解放

	particle_->Finalize();

	delete particle_;

	//=============================================================================================//
	// Camera解放

	delete camera_;
	delete debugCamera_;
}

//=================================================================================================//
// Camera更新

void GameScene::UpdateCamera(Input* input) {

	camera_->SetTransform(cameraTransform_);

	camera_->Update();

	debugCamera_->Update(input);
}

//=================================================================================================//
// Object更新

void GameScene::UpdateObject() {

	//=============================================================================================//
	// Particle更新

	particle_->SetColor(color_);
	particle_->SetBlendMode(particleBlendMode_);
	particle_->SetUseBillboard(useBillboard_);
	particle_->SetUseAccelerationField(useAccelerationField_);
	particle_->Update(debugCamera_->GetViewProjectionMatrix(), debugCamera_->GetCameraMatrix());

#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::Begin("Settings");
	ImGui::ColorEdit4("color", &color_.x);

	static const char* kBlendModeNames[kCountOfBlendMode] = {
		"kBlendModeNone",
		"kBlendModeNormal",
		"kBlendModeAdd",
		"kBlendModeSubtract",
		"kBlendModeMultiply",
		"kBlendModeScreen",
		"kBlendModeLighten",
		"kBlendModeDarken",
		"kBlendModeInvert",
	};

	int blendMode = static_cast<int>(particleBlendMode_);

	if (ImGui::Combo("Blend", &blendMode, kBlendModeNames, kCountOfBlendMode)) {

		particleBlendMode_ = static_cast<BlendMode>(blendMode);
	}

	ImGui::Checkbox("useBillboard", &useBillboard_);

	ImGui::Checkbox("useAccelerationField", &useAccelerationField_);

	ImGui::DragFloat3("EmitterTranslate", &particle_->GetEmitterTransform().translate.x, 0.01f, -100.0f, 100.0f);

	if (ImGui::Button("Add Particle")) {

		particle_->AddParticles();
	}

	ImGui::End();

#endif
}
