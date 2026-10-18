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
	// Object3dRenderer取得

	object3dRenderer_ = context.object3dRenderer;

	//=============================================================================================//
	// モデル初期化

	model_ = new Object3d();

	model_->Initialize(object3dRenderer_, MeshType::kSphere);

	model_->SetTexture("resources/uvChecker.png");

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
	// モデル描画

	model_->Draw();

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
	// モデル解放

	model_->Finalize();

	delete model_;

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
	// カメラ位置をObject3dRendererへ送る

	const Matrix4x4& cameraMatrix = debugCamera_->GetCameraMatrix();

	object3dRenderer_->SetCameraPosition({ cameraMatrix.m[3][0],cameraMatrix.m[3][1],cameraMatrix.m[3][2] });

	//=============================================================================================//
	// モデル更新

	model_->SetTransform(modelTransform_);
	model_->SetColor(modelColor_);
	model_->SetEnableLighting(enableLighting_);
	model_->SetShininess(shininess_);
	model_->SetDirectionalLight(lightColor_, lightDirection_, lightIntensity_);
	model_->SetPointLight(pointLight_);
	spotLight_.direction = MathUtility::Normalize(spotLight_.direction);
	model_->SetSpotLight(spotLight_);
	model_->Update(debugCamera_->GetViewProjectionMatrix());

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

	ImGui::Begin("Model");
	ImGui::DragFloat3("Scale", &modelTransform_.scale.x, 0.01f);
	ImGui::SliderAngle("RotateX", &modelTransform_.rotate.x);
	ImGui::SliderAngle("RotateY", &modelTransform_.rotate.y);
	ImGui::SliderAngle("RotateZ", &modelTransform_.rotate.z);
	ImGui::DragFloat3("Translate", &modelTransform_.translate.x, 0.01f);
	ImGui::ColorEdit4("Color", &modelColor_.x);
	ImGui::Checkbox("EnableLighting", &enableLighting_);
	ImGui::DragFloat("Shininess", &shininess_, 0.1f, 1.0f, 500.0f);
	ImGui::End();

	ImGui::Begin("DirectionalLight");
	ImGui::ColorEdit4("Color", &lightColor_.x);
	ImGui::SliderFloat3("Direction", &lightDirection_.x, -1.0f, 1.0f);
	ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 10.0f);
	ImGui::End();

	ImGui::Begin("PointLight");
	ImGui::ColorEdit4("Color", &pointLight_.color.x);
	ImGui::DragFloat3("Position", &pointLight_.position.x, 0.1f);
	ImGui::DragFloat("Intensity", &pointLight_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Radius", &pointLight_.radius, 0.1f, 0.01f, 100.0f);
	ImGui::DragFloat("Decay", &pointLight_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::End();

	ImGui::Begin("SpotLight");
	ImGui::ColorEdit4("Color", &spotLight_.color.x);
	ImGui::DragFloat3("Position", &spotLight_.position.x, 0.1f);
	ImGui::DragFloat("Intensity", &spotLight_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::SliderFloat3("Direction", &spotLight_.direction.x, -1.0f, 1.0f);
	ImGui::DragFloat("Distance", &spotLight_.distance, 0.1f, 0.01f, 100.0f);
	ImGui::DragFloat("Decay", &spotLight_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::SliderFloat("CosAngle", &spotLight_.cosAngle, 0.0f, 1.0f);
	ImGui::SliderFloat("CosFalloffStart", &spotLight_.cosFalloffStart, 0.0f, 1.0f);
	ImGui::End();

#endif
}
