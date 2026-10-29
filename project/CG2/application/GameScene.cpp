#include "GameScene.h"
#include "WinApp.h"
#include "SceneFile.h"
#include "LogSystem.h"
#include "ModelManager.h"
#include "SceneViewMath.h"
#include <unordered_set>

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
	// Object3dCommon取得

	object3dCommon_ = context.object3dCommon;

	//=============================================================================================//
	// モデル初期化

	ModelManager::GetInstance()->LoadModel("plane.gltf");

	ModelManager::GetInstance()->LoadModel("axis.obj");

	SceneObjectData mainModel;
	mainModel.id = 1;
	mainModel.name = "Plane #1";
	mainModel.transform.scale = {5, 5, 5};
	SceneObjectData axis;
	axis.id = 2;
	axis.name = "Axis #2";
	axis.model = "axis.obj";
	axis.transform.translate = {3, 0, 0};
	std::string sceneError;
	ReplaceObjects({mainModel, axis}, sceneError);
	//=============================================================================================//
	// Particle初期化

	particle_ = new Particle();

	particle_->Initialize(context.particleRenderer);

	particle_->SetTexture("resources/circle.png");

	// Releaseでも保存した編集用データを読み込み、制作時の配置をゲームへ引き継ぐ。
	std::error_code fileError;
	const bool hasSavedScene = std::filesystem::exists(SceneFile::kDefaultPath, fileError);
	if (fileError) {
		LogSystem::Log("Scene file check failed: " + fileError.message() + "\n");
	} else if (hasSavedScene) {
		std::string error;
		if (!SceneFile::Load(*this, SceneFile::kDefaultPath, error)) {
			LogSystem::Log("Scene load failed: " + error + "\n");
		}
	}
	//=============================================================================================//
	// Sprite初期化

	spriteCommon_ = context.spriteCommon;

	sprite_ = new Sprite();

	sprite_->Initialize(spriteCommon_, "resources/uvChecker.png");
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

	object3dCommon_->DrawSettingCommon();

	for (const SceneObjectData& object : GetObjects()) {
		if (object.active && object.hasMeshRenderer) {
			runtimeObjects_.at(object.id).drawable->Draw();
		}
	}

	//=============================================================================================//
	// Particle描画

	particle_->Draw();

	//=============================================================================================//
	// Sprite描画

	spriteCommon_->DrawSettingCommon();

	sprite_->Draw();
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize() {

	//=============================================================================================//
	// Particle解放

	particle_->Finalize();

	delete particle_;

	//=============================================================================================//
	// Sprite解放

	sprite_->Finalize();

	delete sprite_;

	//=============================================================================================//
	// モデル解放

	runtimeObjects_.clear();

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
	// カメラ位置をObject3dCommonへ送る

	const Matrix4x4& cameraMatrix = debugCamera_->GetCameraMatrix();

	object3dCommon_->SetCameraPosition({ cameraMatrix.m[3][0],cameraMatrix.m[3][1],cameraMatrix.m[3][2] });

	//=============================================================================================//
	// モデル更新

	// PostDrawで前フレームのGPU処理が完了しているため、ここで不要な描画リソースを破棄できる。
	// 現フレームの描画命令を記録する前に、設定一覧と描画用一覧を一致させる。
	std::unordered_set<SceneObjectId> liveIds;
	spotLight_.direction = MathUtility::Normalize(spotLight_.direction);
	for (const SceneObjectData& data : GetObjects()) {
		if (!data.active || !data.hasMeshRenderer) {
			continue;
		}
		liveIds.insert(data.id);
		RuntimeObject& runtime = runtimeObjects_[data.id];
		if (!runtime.drawable) {
			runtime.drawable = std::make_unique<Object3d>();
			runtime.drawable->Initialize(object3dCommon_);
		}
		if (runtime.model != data.model) {
			runtime.drawable->SetModel(ModelManager::GetInstance()->FindModel(data.model));
			if (data.model == "plane.gltf") { runtime.drawable->SetTexture("resources/uvChecker.png"); }
			runtime.model = data.model;
		}
		Object3d& object = *runtime.drawable;
		object.SetTransform(data.transform);
		object.SetColor(data.color);
		object.SetEnableLighting(data.enableLighting);
		object.SetShininess(data.shininess);
		object.SetUseRootNodeMatrix(data.useRootNodeMatrix);
		object.SetDirectionalLight(lightColor_, lightDirection_, lightIntensity_);
		object.SetPointLight(pointLight_);
		object.SetSpotLight(spotLight_);
		object.Update(debugCamera_->GetViewProjectionMatrix());
	}
	std::unordered_map<SceneObjectId, RuntimeObject>::iterator runtimeObjectIterator = runtimeObjects_.begin();
	while (runtimeObjectIterator != runtimeObjects_.end()) {
		if (liveIds.contains(runtimeObjectIterator->first)) {
			++runtimeObjectIterator;
		} else {
			runtimeObjectIterator = runtimeObjects_.erase(runtimeObjectIterator);
		}
	}
	//=============================================================================================//
	// Particle更新

	particle_->SetColor(color_);
	particle_->SetBlendMode(particleBlendMode_);
	particle_->SetUseBillboard(useBillboard_);
	particle_->SetUseAccelerationField(useAccelerationField_);
	particle_->Update(debugCamera_->GetViewProjectionMatrix(), debugCamera_->GetCameraMatrix(), simulationEnabled_);

	//=============================================================================================//
	// Sprite更新

	sprite_->SetPosition(spritePosition_);
	sprite_->SetRotation(spriteRotation_);
	sprite_->SetSize(spriteSize_);
	sprite_->Update();

#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::Begin("Settings");
	ImGui::BeginDisabled(playing_);
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
	ImGui::EndDisabled();

	ImGui::BeginDisabled(!simulationEnabled_);
	if (ImGui::Button("Add Particle")) {

		particle_->AddParticles();
	}
	ImGui::EndDisabled();

	ImGui::End();

	ImGui::Begin("DirectionalLight");
	ImGui::BeginDisabled(playing_);
	ImGui::ColorEdit4("Color", &lightColor_.x);
	ImGui::SliderFloat3("Direction", &lightDirection_.x, -1.0f, 1.0f);
	ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 10.0f);
	ImGui::EndDisabled();
	ImGui::End();

	ImGui::Begin("PointLight");
	ImGui::BeginDisabled(playing_);
	ImGui::ColorEdit4("Color", &pointLight_.color.x);
	ImGui::DragFloat3("Position", &pointLight_.position.x, 0.1f);
	ImGui::DragFloat("Intensity", &pointLight_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::DragFloat("Radius", &pointLight_.radius, 0.1f, 0.01f, 100.0f);
	ImGui::DragFloat("Decay", &pointLight_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::EndDisabled();
	ImGui::End();

	ImGui::Begin("SpotLight");
	ImGui::BeginDisabled(playing_);
	ImGui::ColorEdit4("Color", &spotLight_.color.x);
	ImGui::DragFloat3("Position", &spotLight_.position.x, 0.1f);
	ImGui::DragFloat("Intensity", &spotLight_.intensity, 0.01f, 0.0f, 10.0f);
	ImGui::SliderFloat3("Direction", &spotLight_.direction.x, -1.0f, 1.0f);
	ImGui::DragFloat("Distance", &spotLight_.distance, 0.1f, 0.01f, 100.0f);
	ImGui::DragFloat("Decay", &spotLight_.decay, 0.01f, 0.0f, 10.0f);
	ImGui::SliderFloat("CosAngle", &spotLight_.cosAngle, 0.0f, 1.0f);
	ImGui::SliderFloat("CosFalloffStart", &spotLight_.cosFalloffStart, 0.0f, 1.0f);
	ImGui::EndDisabled();
	ImGui::End();

	ImGui::Begin("Sprite");
	ImGui::BeginDisabled(playing_);
	ImGui::SliderFloat2("Position", &spritePosition_.x, 0.f, static_cast<float>(WinApp::kClientWidth));
	ImGui::SliderAngle("Rotation", &spriteRotation_);
	ImGui::SliderFloat2("Size", &spriteSize_.x, 0.f, 512.f);
	ImGui::EndDisabled();
	ImGui::End();

#endif
}

// 編集・一時停止中もカメラと描画は更新し、ゲーム固有の更新だけを止める。
void GameScene::UpdateEditorFrame(Input* input, bool simulate) {
	simulationEnabled_ = simulate;
	if (simulate) {
		Update(input);
	} else {
		UpdateCamera(input);
		UpdateObject();
	}
}

// 再生開始・停止時に粒子と発生タイマーを消し、毎回同じ状態から確認できるようにする。
void GameScene::OnPlayStateChanged(bool playing) {
	playing_ = playing;
	particle_->ResetSimulation();
}

// 現在描画しているDebugCameraの行列と、DX12で使う固定の表示サイズを公開する。
bool GameScene::GetEditorViewInfo(EditorViewInfo& viewInfo) const {
    if (debugCamera_ == nullptr) {
        return false;
    }
    viewInfo.viewProjection = debugCamera_->GetViewProjectionMatrix();
    const Matrix4x4& cameraMatrix = debugCamera_->GetCameraMatrix();
    viewInfo.cameraPosition = {cameraMatrix.m[3][0], cameraMatrix.m[3][1], cameraMatrix.m[3][2]};
    viewInfo.viewportWidth = static_cast<float>(WinApp::kClientWidth);
    viewInfo.viewportHeight = static_cast<float>(WinApp::kClientHeight);
    return true;
}

// メッシュの範囲へ描画時と同じRootNode・Transformを適用し、選択位置のずれを防ぐ。
bool GameScene::GetEditableObjectBounds(size_t objectIndex, AABB& worldBounds) const {
    if (objectIndex >= GetObjects().size()) {
        return false;
    }
    const SceneObjectData& object = GetObjects()[objectIndex];
    if (!object.active || !object.hasMeshRenderer) {
        return false;
    }
    const Model* model = ModelManager::GetInstance()->FindModel(object.model);
    if (model == nullptr) {
        return false;
    }
    Matrix4x4 worldMatrix = MathUtility::MakeAffineMatrix(object.transform.scale,
        object.transform.rotate, object.transform.translate);
    if (object.useRootNodeMatrix) {
        worldMatrix = MathUtility::Multiply(model->GetRootNode().localMatrix, worldMatrix);
    }
    worldBounds = SceneViewMath::TransformBounds(model->GetLocalBounds(), worldMatrix);
    return true;
}

// Editorの注視要求を、シーンが所有するカメラへ渡す。
void GameScene::FocusEditorCamera(const Vector3& position) {
    if (debugCamera_ != nullptr) {
        debugCamera_->Focus(position);
    }
}
