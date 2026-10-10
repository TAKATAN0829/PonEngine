#include "GameScene.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=================================================================================================//
// 初期化処理

void GameScene::Initialize (const SceneContext& context) {

	//=============================================================================================//
	// Camera初期化

	camera_ = new Camera ();

	camera_->Initialize (WinApp::kClientWidth, WinApp::kClientHeight);

	cameraTransform_ = camera_->GetTransform ();

	cameraTransform_.translate = { 0.f,0.f,-30.f };

	debugCamera_ = new DebugCamera ();

	debugCamera_->Initialize (WinApp::kClientWidth, WinApp::kClientHeight);

	//=============================================================================================//
	// Particle初期化

	particle_ = new Particle ();

	particle_->Initialize (context.particleRenderer);

	//=============================================================================================//
	// Fence初期化

	fence_ = new Object3d ();

	fence_->Initialize (
		context.object3dRenderer,
		"resources/fence",
		"fence.obj");

	fence_->SetTexture ("resources/fence/fence.png");

	//=============================================================================================//
	// SpriteCommon取得

	spriteCommon_ = context.spriteCommon;
}

//=================================================================================================//
// 更新処理

void GameScene::Update (Input* input) {

	input;

	// Camera更新
	UpdateCamera (input);

	// Object更新
	UpdateObject ();
}

//=================================================================================================//
// 描画処理

void GameScene::Draw () {

	//=============================================================================================//
	// Particle描画

	particle_->Draw ();

	//=============================================================================================//
	// Fence描画

	fence_->Draw ();

	//=============================================================================================//
	// Sprite描画

	// Spriteの描画準備。Spriteの描画に共通のグラフィックスコマンドを積む
	spriteCommon_->DrawSettingCommon ();

	// TODO: 全てのSprite個々の描画
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize () {

	//=============================================================================================//
	// Particle解放

	particle_->Finalize ();

	delete particle_;

	//=============================================================================================//
	// Fence解放

	fence_->Finalize ();

	delete fence_;

	//=============================================================================================//
	// Camera解放

	delete camera_;
	delete debugCamera_;

}

//=================================================================================================//
// Camera更新

void GameScene::UpdateCamera (Input* input) {

	camera_->SetTransform (cameraTransform_);

	camera_->Update ();

	debugCamera_->Update (input);
}

//=================================================================================================//
// Object更新

void GameScene::UpdateObject () {

	//=============================================================================================//
	// Particle更新

	particle_->SetColor (color_);
	particle_->SetBlendMode (blendMode_);
	particle_->Update (debugCamera_->GetViewProjectionMatrix ());

	//=============================================================================================//
	// Fence更新

	fence_->SetTransform (fenceTransform_);
	fence_->SetBlendMode (blendMode_);
	fence_->Update (debugCamera_->GetViewProjectionMatrix ());

#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::Begin ("Settings");
	ImGui::ColorEdit4 ("color", &color_.x);

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

	int blendMode = static_cast<int>(blendMode_);

	if (ImGui::Combo (
		"Blend",
		&blendMode,
		kBlendModeNames,
		kCountOfBlendMode)) {

		blendMode_ = static_cast<BlendMode>(blendMode);
	}

	ImGui::End ();

	ImGui::Begin ("Fence");
	ImGui::DragFloat3 (
		"FenceRotate",
		&fenceTransform_.rotate.x,
		0.01f);
	ImGui::DragFloat3 (
		"FenceTranslate",
		&fenceTransform_.translate.x,
		0.1f);
	ImGui::End ();

#endif
}