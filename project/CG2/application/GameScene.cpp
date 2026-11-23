#include "GameScene.h"
#include "WinApp.h"
#include "ModelManager.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"

#endif

//=================================================================================================//
// 初期化処理

void GameScene::Initialize(const SceneContext& context) {

	//===========================================//
	// ModelCollection取得
	//===========================================//

	modelCollection_ = context.modelCollection;

	//===========================================//
	// ModelRenderer取得
	//===========================================//

	modelRenderer_ = context.modelRenderer;

	//===========================================//
	// Camera初期化
	//===========================================//

	camera_ = new Camera();
	camera_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);
	cameraTransform_ = camera_->GetTransform();
	cameraTransform_.translate = { 0.f,0.f,-30.f };

	//===========================================//
	// DebugCamera初期化
	//===========================================//

	debugCamera_ = new DebugCamera();
	debugCamera_->Initialize(WinApp::kClientWidth, WinApp::kClientHeight);

	//===========================================//
	// Object3dPipeline取得
	//===========================================//

	object3dPipeline_ = context.object3dPipeline;

	//===========================================//
	// モデル初期化
	//===========================================//

	//==================//
	// メインモデル

	// 設定
	modelIndex_ = 0;

	// 生成
	playerId_ = modelCollection_->AddModel("Player", "plane.gltf", "resources/uvChecker.png");

	// 設定
	SceneModel* player = modelCollection_->FindModel(playerId_);
	if (player != nullptr) {
		player->transform.scale = {5.0f, 5.0f, 5.0f};
	}

	//==================//
	// サブモデル

	// 生成
	subModelId_ = modelCollection_->AddModel("Axis", "axis.obj", "");

	// 設定
	SceneModel* subModel = modelCollection_->FindModel(subModelId_);
	if (subModel != nullptr) {
		subModel->transform.translate = {3.0f, 0.0f, 0.0f};
	}

	//==================//
	// 描画用Object3dの登録

	for (const SceneModel& model : modelCollection_->GetModels()) {
		modelRenderer_->AddModel(model);
	}

	//===========================================//
	// Particle初期化
	//===========================================//

	// 設定
	color_ = { 1.0f,1.0f,1.0f,1.0f };
	particleBlendMode_ = kBlendModeAdd;
	useBillboard_ = true;
	useAccelerationField_ = false;

	// 生成
	particle_ = new Particle();
	particle_->Initialize(context.particleRenderer);
	particle_->SetTexture("resources/circle.png");

	//===========================================//
	// Sprite初期化
	//===========================================//

	// 設定
	spritePosition_ = { 100.f,100.f };
	spriteRotation_ = 0.f;
	spriteSize_ = { 256.f,256.f };

	// 描画機能の取得
	spriteCommon_ = context.spriteCommon;

	// 生成
	sprite_ = new Sprite();
	sprite_->Initialize(spriteCommon_, "resources/uvChecker.png");

	//===========================================//
	// 平行光源初期化
	//===========================================//

	lightColor_ = { 1.0f,1.0f,1.0f,1.0f };
	lightDirection_ = { 0.0f,-1.0f,0.0f };
	lightIntensity_ = 1.0f;

	//===========================================//
	// 点光源初期化
	//===========================================//

	pointLight_ = {
		{ 1.0f,1.0f,1.0f,1.0f },
		{ 0.0f,2.0f,0.0f },
		1.0f,
		10.0f,
		1.0f
	};

	//===========================================//
	// スポットライト初期化
	//===========================================//

	spotLight_ = {
		{ 1.0f,1.0f,1.0f,1.0f },
		{ 2.0f,1.25f,0.0f },
		4.0f,
		{ -1.0f,-1.0f,0.0f },
		7.0f,
		2.0f,
		0.5f,
		1.0f
	};
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

	//===========================================//
	// モデル描画
	//===========================================//

	modelRenderer_->Draw(*modelCollection_);

	//===========================================//
	// Particle描画
	//===========================================//

	particle_->Draw();

	//===========================================//
	// Sprite描画
	//===========================================//

	spriteCommon_->DrawSettingCommon();
	sprite_->Draw();
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize() {

	//===========================================//
	// Particle解放
	//===========================================//

	particle_->Finalize();
	delete particle_;

	//===========================================//
	// Sprite解放
	//===========================================//

	sprite_->Finalize();
	delete sprite_;

	//===========================================//
	// Camera解放
	//===========================================//

	delete camera_;

	//===========================================//
	// DebugCamera解放
	//===========================================//

	delete debugCamera_;
}

//=================================================================================================//
// Camera更新

void GameScene::UpdateCamera(Input* input) {

	//===========================================//
	// Camera更新
	//===========================================//

	camera_->SetTransform(cameraTransform_);
	camera_->Update();

	//===========================================//
	// DebugCamera更新
	//===========================================//

	debugCamera_->Update(input);
}

//=================================================================================================//
// Object更新

void GameScene::UpdateObject() {

	//===========================================//
	// カメラ位置をObject3dPipelineへ送る
	//===========================================//

	const Matrix4x4& cameraMatrix = debugCamera_->GetCameraMatrix();
	object3dPipeline_->SetCameraPosition({ cameraMatrix.m[3][0],cameraMatrix.m[3][1],cameraMatrix.m[3][2] });

	//===========================================//
	// モデル更新
	//===========================================//

	// ModelCollectionに合わせて描画物を更新する
	modelRenderer_->Refresh(*modelCollection_);

	// ライト設定
	SceneModel* player = modelCollection_->FindModel(playerId_);
	Object3d* playerObject = modelRenderer_->FindObject(playerId_);
	if (playerObject != nullptr) {
		playerObject->SetDirectionalLight(lightColor_, lightDirection_, lightIntensity_);
		playerObject->SetPointLight(pointLight_);
		spotLight_.direction = MathUtility::Normalize(spotLight_.direction);
		playerObject->SetSpotLight(spotLight_);
	}

	// 全モデルのデータ反映と行列更新
	modelRenderer_->Update(*modelCollection_, debugCamera_->GetViewProjectionMatrix());

	//===========================================//
	// Particle更新
	//===========================================//

	particle_->SetColor(color_);
	particle_->SetBlendMode(particleBlendMode_);
	particle_->SetUseBillboard(useBillboard_);
	particle_->SetUseAccelerationField(useAccelerationField_);
	particle_->Update(debugCamera_->GetViewProjectionMatrix(), debugCamera_->GetCameraMatrix());

	//===========================================//
	// Sprite更新
	//===========================================//

	sprite_->SetPosition(spritePosition_);
	sprite_->SetRotation(spriteRotation_);
	sprite_->SetSize(spriteSize_);
	sprite_->Update();

#ifdef USE_IMGUI

	//===========================================//
	// Particle設定UI
	//===========================================//

	ImGui::Begin("Settings");
	ImGui::ColorEdit4("color", &color_.x);

	// ブレンド設定
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

	// 発生設定
	ImGui::Checkbox("useBillboard", &useBillboard_);
	ImGui::Checkbox("useAccelerationField", &useAccelerationField_);
	ImGui::DragFloat3("EmitterTranslate", &particle_->GetEmitterTransform().translate.x, 0.01f, -100.0f, 100.0f);

	// パーティクル追加
	if (ImGui::Button("Add Particle")) {
		particle_->AddParticles();
	}
	ImGui::End();

	//===========================================//
	// メインモデル設定UI
	//===========================================//

	ImGui::Begin("Model Asset");
	static const char* kModelNames[] = { "plane.gltf", "axis.obj" };
	if (ImGui::Combo("ModelData", &modelIndex_, kModelNames, IM_ARRAYSIZE(kModelNames))) {
		if (player != nullptr && playerObject != nullptr) {
			player->modelFile = kModelNames[modelIndex_];
			playerObject->SetModel(ModelManager::GetInstance()->FindModel(player->modelFile));
		}
	}
	if (player != nullptr) {
		ImGui::Checkbox("UseRootNodeMatrix", &player->useRootNodeMatrix);
	}
	ImGui::End();

	//===========================================//
	// 平行光源設定UI
	//===========================================//

	ImGui::Begin("DirectionalLight");
	ImGui::ColorEdit4("Color", &lightColor_.x);
	ImGui::SliderFloat3("Direction", &lightDirection_.x, -1.0f, 1.0f);
	ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 10.0f);
	ImGui::End();

	//===========================================//
	// 点光源設定UI
	//===========================================//

	ImGui::Begin("PointLight");
	ImGui::ColorEdit4("Color", &pointLight_.color.x);
	ImGui::DragFloat3("Position", &pointLight_.position.x, 0.1f);
	ImGui::DragFloat("Intensity", &pointLight_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Radius", &pointLight_.radius, 0.1f, 0.01f, 100.0f);
	ImGui::DragFloat("Decay", &pointLight_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::End();

	//===========================================//
	// スポットライト設定UI
	//===========================================//

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

	//===========================================//
	// Sprite設定UI
	//===========================================//

	ImGui::Begin("Sprite");
	ImGui::SliderFloat2("Position", &spritePosition_.x, 0.f, static_cast<float>(WinApp::kClientWidth));
	ImGui::SliderAngle("Rotation", &spriteRotation_);
	ImGui::SliderFloat2("Size", &spriteSize_.x, 0.f, 512.f);
	ImGui::End();

#endif
}
