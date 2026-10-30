#include "FrameProfiler.h"
#include "GameScene.h"
#include "WinApp.h"
#include "SceneFile.h"
#include "ParticlePresetFile.h"
#include "LogSystem.h"
#include "ModelManager.h"
#include "SceneViewMath.h"
#include "TextureManager.h"
#include <unordered_set>
#include <limits>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=================================================================================================//
// 初期化処理

void GameScene::Initialize(const SceneContext& context) {
#ifndef USE_IMGUI
	gameView_ = true; // Releaseも保存したメインカメラを使う。
#endif

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

	particleRenderer_ = context.particleRenderer;
	particle_ = new Particle();

	particle_->Initialize(context.particleRenderer);

	particle_->SetTexture("resources/circle.png");
	std::error_code presetError;
	if (std::filesystem::exists(ParticlePresetFile::kDefaultPath, presetError)) {
		std::string presetMessage;
		if (!ParticlePresetFile::Load(particleSettings_, ParticlePresetFile::kDefaultPath, presetMessage)) {
			LogSystem::Log("Particle preset: " + presetMessage + "\n");
		}
	}

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

void GameScene::Update(Input*) {

	// Camera更新
	UpdateCamera();

	// Object更新
	UpdateObject();
}

//=================================================================================================//
// 描画処理

void GameScene::Draw() {

	//=============================================================================================//
	// モデル描画

	const bool isolateParticles = particlePreview_.enabled && particlePreview_.isolate && !playing_;
	if (!isolateParticles) {
		object3dCommon_->DrawSettingCommon();

		for (const SceneObjectData& object : GetObjects()) {
			if (IsObjectActive(object.id)) {
				if (object.hasMeshRenderer) {
					runtimeObjects_.at(object.id).drawable->Draw();
				}
			}
		}

	}

	//=============================================================================================//
	// Particle描画

	particle_->Draw();
	for (const std::pair<const SceneObjectId, std::unique_ptr<Particle>>& entry : runtimeParticles_) {
		entry.second->Draw();
	}

	//=============================================================================================//
	// Sprite描画

#ifdef USE_IMGUI
	if (gameView_) {
		if (!isolateParticles) {
#endif
			spriteCommon_->DrawSettingCommon();

			sprite_->Draw();
#ifdef USE_IMGUI
		}
	}
#endif
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize() {
	for (std::pair<const std::string,std::unique_ptr<ModelPreview>>& entry : modelPreviews_) {
		entry.second->target.Finalize();
	}
	modelPreviews_.clear();

	//=============================================================================================//
	// Particle解放

	for (std::pair<const SceneObjectId, std::unique_ptr<Particle>>& entry : runtimeParticles_) {
		entry.second->Finalize();
	}
	runtimeParticles_.clear();
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

void GameScene::UpdateCamera() {

	camera_->SetTransform(cameraTransform_);

	camera_->Update();

	debugCamera_->Update();
}

//=================================================================================================//
// Object更新

// カメラ情報だけをここで用意し、モデル・Emitter・Spriteの更新へ渡す。
void GameScene::UpdateObject() {
	Matrix4x4 cameraMatrix = debugCamera_->GetCameraMatrix();
	Matrix4x4 viewProjection = debugCamera_->GetViewProjectionMatrix();
	if (gameView_) {
		cameraMatrix = MathUtility::Inverse(camera_->GetViewMatrix());
		viewProjection = camera_->GetViewProjectionMatrix();
		SceneCameraView cameraView{};
		if (GetGameCameraView(static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_), cameraView)) {
			cameraMatrix = cameraView.cameraWorld;
			viewProjection = cameraView.viewProjection;
		}
	}
	object3dCommon_->SetCameraPosition({cameraMatrix.m[3][0], cameraMatrix.m[3][1], cameraMatrix.m[3][2]});
	{
		FrameProfiler::Scope sectionTime(FrameProfiler::Section::Previews);
		DrawModelPreviews();
	}
	{
		FrameProfiler::Scope sectionTime(FrameProfiler::Section::Models);
		UpdateModels(viewProjection);
	}
	{
		FrameProfiler::Scope sectionTime(FrameProfiler::Section::Particles);
		UpdateParticles(viewProjection, cameraMatrix);
	}
	UpdateSprite();
}

//=============================================================================================//
// Getter / 描画用の点光源一覧

std::vector<PointLight> GameScene::GetPointLights() const {
	std::vector<PointLight> lights = {pointLight_};
	for (const SceneObjectData& data : GetObjects()) {
		if (!IsObjectActive(data.id)) {
			continue;
		}
		if (data.type == SceneObjectType::PointLight) {
			PointLight light{};
			light.position = GetObjectWorldPosition(data.id);
			light.color = data.color;
			light.intensity = data.lightIntensity;
			light.radius = data.lightRadius;
			light.decay = data.lightDecay;
			lights.push_back(light);
		}
	}
	return lights;
}

//=============================================================================================//
// 更新処理 / モデル

void GameScene::UpdateModels(const Matrix4x4& viewProjection) {
	// 前フレームのGPU完了後、描画命令を記録する前に不要なリソースを破棄する。
	const std::vector<PointLight> pointLights = GetPointLights();
	std::unordered_set<SceneObjectId> liveIds;
	spotLight_.direction = MathUtility::Normalize(spotLight_.direction);
	for (const SceneObjectData& data : GetObjects()) {
		if (!IsObjectActive(data.id)) {
			continue;
		}
		if (!data.hasMeshRenderer) {
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
			if (data.model == "plane.gltf") {
				runtime.drawable->SetTexture("resources/uvChecker.png");
			}
			runtime.model = data.model;
		}
		Object3d& object = *runtime.drawable;
		object.SetTransform(data.transform);
		object.SetColor(data.color);
		object.SetEnableLighting(data.enableLighting);
		object.SetShininess(data.shininess);
		object.SetUseRootNodeMatrix(data.useRootNodeMatrix);
		object.SetDirectionalLight(lightColor_, lightDirection_, lightIntensity_);
		object.SetPointLights(pointLights);
		object.SetSpotLight(spotLight_);
		object.SetMaterials(data.materials);
		bool animate = runtime.animationPreview;
		if (playing_) {
			animate = simulationEnabled_ && data.animation.autoplay;
		}
#ifndef USE_IMGUI
		animate = simulationEnabled_ && data.animation.autoplay;
#endif
		object.UpdateAnimation(data.animation, 1.0f / 60.0f, animate);
		Matrix4x4 world{};
		if (GetObjectWorldMatrix(data.id, world)) {
			object.Update(viewProjection, world);
		}
	}
	std::unordered_map<SceneObjectId, RuntimeObject>::iterator runtimeObjectIterator = runtimeObjects_.begin();
	while (runtimeObjectIterator != runtimeObjects_.end()) {
		if (liveIds.contains(runtimeObjectIterator->first)) {
			++runtimeObjectIterator;
		} else {
			runtimeObjectIterator = runtimeObjects_.erase(runtimeObjectIterator);
		}
	}
}

//=============================================================================================//
// Getter / Particleの更新・発生可否

bool GameScene::IsParticlePreviewTarget(SceneObjectId objectId) const {
	if (playing_) {
		return false;
	}
	if (!particlePreview_.enabled) {
		return false;
	}
	return particlePreview_.targetObject == objectId;
}

bool GameScene::ShouldSimulateEmitter(SceneObjectId objectId, bool gameEmissionEnabled) const {
	if (simulationEnabled_) {
		if (gameEmissionEnabled) {
			return true;
		}
	}
	if (IsParticlePreviewTarget(objectId)) {
		return !particlePreview_.paused || particlePreview_.step;
	}
	return false;
}

bool GameScene::ShouldEmitEmitter(SceneObjectId objectId, bool gameEmissionEnabled) const {
	if (simulationEnabled_) {
		if (gameEmissionEnabled) {
			return true;
		}
	}
	if (IsParticlePreviewTarget(objectId)) {
		return particlePreview_.automaticEmission;
	}
	return false;
}

//=============================================================================================//
// 更新処理 / 個別Emitter

void GameScene::UpdateEmitter(const SceneObjectData& data, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix) {
	std::unique_ptr<Particle>& emitter = runtimeParticles_[data.id];
	if (!emitter) {
		emitter = std::make_unique<Particle>();
		emitter->Initialize(particleRenderer_);
	}
	emitter->ApplySettings(data.particle);
	emitter->SetEmitterPosition(GetObjectWorldPosition(data.id));
	if (particleBurstRequested_) {
		if (particlePreview_.targetObject == data.id) {
			emitter->AddParticles();
		}
	}
	emitter->Update(viewProjection, cameraMatrix, ShouldSimulateEmitter(data.id, true), ShouldEmitEmitter(data.id, true));
}

//=============================================================================================//
// 更新処理 / Particle一覧

void GameScene::UpdateParticles(const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix) {
	std::unordered_set<SceneObjectId> particleIds;
	for (const SceneObjectData& data : GetObjects()) {
		if (!IsObjectActive(data.id)) {
			continue;
		}
		if (data.type != SceneObjectType::Particle) {
			continue;
		}
		particleIds.insert(data.id);
		UpdateEmitter(data, viewProjection, cameraMatrix);
	}
	std::unordered_map<SceneObjectId, std::unique_ptr<Particle>>::iterator emitterIterator = runtimeParticles_.begin();
	while (emitterIterator != runtimeParticles_.end()) {
		if (particleIds.contains(emitterIterator->first)) {
			++emitterIterator;
		}
		else {
			emitterIterator->second->Finalize();
			emitterIterator = runtimeParticles_.erase(emitterIterator);
		}
	}
	// 個別Emitterがあるゲームでは共通プリセットの自動発生を止め、二重発生を防ぐ。
	particle_->ApplySettings(particleSettings_);
	if (particleBurstRequested_) {
		if (particlePreview_.targetObject == kInvalidSceneObjectId) {
			particle_->AddParticles();
		}
	}
	particleBurstRequested_ = false;
	if (playing_) {
		if (!runtimeParticles_.empty()) {
			particle_->ResetSimulation();
		}
	}
	particle_->Update(viewProjection, cameraMatrix,
		ShouldSimulateEmitter(kInvalidSceneObjectId, runtimeParticles_.empty()),
		ShouldEmitEmitter(kInvalidSceneObjectId, runtimeParticles_.empty()));
	particlePreview_.step = false;
}

//=============================================================================================//
// 更新処理 / Sprite

void GameScene::UpdateSprite() {
	sprite_->SetPosition(spritePosition_);
	sprite_->SetRotation(spriteRotation_);
	sprite_->SetSize(spriteSize_);
	sprite_->Update();
}

//=============================================================================================//
// 描画処理 / World Settings

void GameScene::DrawWorldSettings(bool* open) {
#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::SetNextWindowSize({390, 480}, ImGuiCond_FirstUseEver);
	if (!ImGui::Begin("World Settings", open)) {
		ImGui::End();
		return;
	}
	if (ImGui::CollapsingHeader("DirectionalLight")) {

		ImGui::BeginDisabled(playing_);
		ImGui::ColorEdit4("Color", &lightColor_.x);
		ImGui::SliderFloat3("Direction", &lightDirection_.x, -1.0f, 1.0f);
		ImGui::DragFloat("Intensity", &lightIntensity_, 0.01f, 0.0f, 10.0f);
		ImGui::EndDisabled();

	}
	if (ImGui::CollapsingHeader("PointLight")) {

		ImGui::BeginDisabled(playing_);
		ImGui::ColorEdit4("Color", &pointLight_.color.x);
		ImGui::DragFloat3("Position", &pointLight_.position.x, 0.1f);
		ImGui::DragFloat("Intensity", &pointLight_.intensity, 0.01f, 0.0f, 10.0f);
		ImGui::DragFloat("Radius", &pointLight_.radius, 0.1f, 0.01f, 100.0f);
		ImGui::DragFloat("Decay", &pointLight_.decay, 0.01f, 0.0f, 10.0f);
		ImGui::EndDisabled();

	}
	if (ImGui::CollapsingHeader("SpotLight")) {

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

	}
	if (ImGui::CollapsingHeader("Sprite")) {

		ImGui::BeginDisabled(playing_);
		ImGui::SliderFloat2("Position", &spritePosition_.x, 0.f, static_cast<float>(WinApp::kClientWidth));
		ImGui::SliderAngle("Rotation", &spriteRotation_);
		ImGui::SliderFloat2("Size", &spriteSize_.x, 0.f, 512.f);
		ImGui::EndDisabled();

	}
	ImGui::End();

#else
	(void)open;
#endif
}

// 編集・一時停止中もカメラと描画は更新し、ゲーム固有の更新だけを止める。
void GameScene::UpdateEditorFrame(Input* input, bool simulate) {
	simulationEnabled_ = simulate;
	if (simulate) {
		Update(input);
	} else {
		UpdateCamera();
		UpdateObject();
	}
}

// 再生開始・停止時に粒子と発生タイマーを消し、毎回同じ状態から確認できるようにする。
void GameScene::OnPlayStateChanged(bool playing) {
	playing_ = playing;
	for (std::pair<const SceneObjectId, RuntimeObject>& entry : runtimeObjects_) {
		entry.second.animationPreview = false;
		entry.second.drawable->ResetAnimation();
	}
	ResetParticlePreview();
}

// Sceneパネル内のカメラと描画サイズを公開し、画像とクリック位置を一致させる。
bool GameScene::GetEditorViewInfo(EditorViewInfo& viewInfo) const {
	if (debugCamera_ == nullptr) {
		return false;
	}
	viewInfo.viewProjection = debugCamera_->GetViewProjectionMatrix();
	const Matrix4x4& cameraMatrix = debugCamera_->GetCameraMatrix();
	viewInfo.cameraPosition = {cameraMatrix.m[3][0], cameraMatrix.m[3][1], cameraMatrix.m[3][2]};
	viewInfo.viewportWidth = static_cast<float>(viewportWidth_);
	viewInfo.viewportHeight = static_cast<float>(viewportHeight_);
	return true;
}
// メッシュの範囲へ描画時と同じRootNode・Transformを適用し、選択位置のずれを防ぐ。
bool GameScene::GetEditableObjectBounds(size_t objectIndex, AABB& worldBounds) const {
	if (objectIndex >= GetObjects().size()) {
		return false;
	}
	const SceneObjectData& object = GetObjects()[objectIndex];
	if (!IsObjectActive(object.id)) {
		return false;
	}
	if (!object.hasMeshRenderer) {
		if (object.type == SceneObjectType::Model) {
			return false;
		}
		// メッシュを持たない対象も選択・注視できる小さな範囲を公開する。
		const Vector3 position = GetObjectWorldPosition(object.id);
		worldBounds = {{position.x-0.3f,position.y-0.3f,position.z-0.3f}, {position.x+0.3f,position.y+0.3f,position.z+0.3f}};
		return true;
	}
	const Model* model = ModelManager::GetInstance()->FindModel(object.model);
	if (model == nullptr) {
		return false;
	}
	Matrix4x4 worldMatrix{};
	if (!GetObjectWorldMatrix(object.id, worldMatrix)) {
		return false;
	}
	if (object.useRootNodeMatrix) {
		worldMatrix = MathUtility::Multiply(model->GetRootNode().localMatrix, worldMatrix);
	}
	const AABB* localBounds = &model->GetLocalBounds();
	if (runtimeObjects_.contains(object.id)) {
		localBounds = &runtimeObjects_.at(object.id).drawable->GetLocalBounds();
	}
	worldBounds = SceneViewMath::TransformBounds(*localBounds, worldMatrix);
	return true;
}

// Editorの注視要求を、シーンが所有するカメラへ渡す。
void GameScene::FocusEditorCamera(const Vector3& position) {
	if (debugCamera_ != nullptr) {
		debugCamera_->Focus(position);
	}
}

// 表示領域の大きさを投影行列と2D描画へ同時に渡す。
void GameScene::SetEditorViewport(uint32_t width, uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}
	// 変わっていない表示サイズではカメラ・Spriteの投影行列を作り直さない。
	if (viewportWidth_ == width && viewportHeight_ == height) {
		return;
	}
	viewportWidth_ = width;
	viewportHeight_ = height;
	debugCamera_->SetViewportSize(static_cast<int32_t>(width), static_cast<int32_t>(height));
	camera_->SetViewportSize(static_cast<int32_t>(width), static_cast<int32_t>(height));
	if (spriteCommon_ != nullptr) {
		spriteCommon_->SetViewportSize(static_cast<float>(width), static_cast<float>(height));
	}
}

// Scene画像の上にいるときだけEditorカメラの入力を受け付ける。
void GameScene::SetEditorCameraInput(bool enabled) {
	debugCamera_->SetEditorInputEnabled(enabled);
}

// プレビューの再開や設定読込時は、古い粒子を残さず作り直す。
void GameScene::ResetParticlePreview() {
	particle_->ResetSimulation();
	for (std::pair<const SceneObjectId, std::unique_ptr<Particle>>& entry : runtimeParticles_) {
		entry.second->ResetSimulation();
	}
}

// 表示上限だけではなく、実際に管理している粒子数をEditorへ通知する。
size_t GameScene::GetParticleCount() const {
	if (particlePreview_.targetObject != kInvalidSceneObjectId) {
		const std::unordered_map<SceneObjectId, std::unique_ptr<Particle>>::const_iterator emitter = runtimeParticles_.find(particlePreview_.targetObject);
		if (emitter != runtimeParticles_.end()) {
			return emitter->second->GetLiveCount();
		}
		return 0;
	}
	return particle_->GetLiveCount();
}

// 表示状態はEditorが所有し、閉じるボタンの結果もこの呼出中に返す。
void GameScene::DrawEditorTools(EditorToolVisibility& visibility) {
	if (visibility.worldSettings) {
		DrawWorldSettings(&visibility.worldSettings);
	}
	if (visibility.camera) {
		debugCamera_->Draw(&visibility.camera);
	}
}

bool GameScene::PrepareModel(const std::string& model, std::string& error) {
	if (!SceneObjects::IsSupportedModel(model)) {
		error = "Unsupported model path.";
		return false;
	}
	return ModelManager::GetInstance()->TryLoadModel(model,error);
}

bool GameScene::ReplaceObjects(std::vector<SceneObjectData> objects, std::string& error) {
	SceneObjects validator;
	if (!validator.ReplaceObjects(objects,error)) {
		return false;
	}
	for (const SceneObjectData& object : objects) {
		// Mesh Rendererを外して保存したモデルも、後から再追加できるよう準備する。
		if (!PrepareModel(object.model, error)) {
			return false;
		}
		const size_t materialCount = GetModelMaterials(object.model).size();
		for (const SceneMaterialSettings& material : object.materials) {
			if (material.slot >= materialCount) {
				error = "Material slot does not exist in this model.";
				return false;
			}
			if (!material.texture.empty()) {
				if (!PrepareTexture(material.texture, error)) {
					return false;
				}
			}
		}
	}
	return SceneObjects::ReplaceObjects(std::move(objects),error);
}

std::vector<ModelMaterialInfo> GameScene::GetModelMaterials(const std::string& model) const {
	const Model* loaded = ModelManager::GetInstance()->FindModel(model);
	if (loaded == nullptr) {
		return {};
	}
	return loaded->GetMaterials();
}

std::vector<ModelAnimationInfo> GameScene::GetModelAnimations(const std::string& model) const {
	const Model* loaded = ModelManager::GetInstance()->FindModel(model);
	if (loaded == nullptr) {
		return {};
	}
	if (loaded->GetAnimation() == nullptr) {
		return {};
	}
	return loaded->GetAnimation()->GetClips();
}

bool GameScene::PrepareTexture(const std::string& texture, std::string& error) {
	error.clear();
	const std::filesystem::path relative(std::u8string(texture.begin(), texture.end()));
	if (texture.empty() || texture.size() > 255 || texture.find('\0') != std::string::npos || relative.is_absolute() || relative.has_root_name()) {
		error = "Invalid texture path.";
		return false;
	}
	for (const std::filesystem::path& part : relative) {
		if (part == "..") {
			error = "Texture must be inside resources.";
			return false;
		}
	}
	std::error_code fileError;
	const std::filesystem::path root = std::filesystem::weakly_canonical("resources", fileError);
	const std::filesystem::path resolved = std::filesystem::weakly_canonical(root / relative, fileError);
	const std::filesystem::path inside = resolved.lexically_relative(root);
	if (fileError || inside.empty()) {
		error = "Cannot resolve texture path.";
		return false;
	}
	if (*inside.begin() == "..") {
		error = "Texture must be inside resources.";
		return false;
	}
	return TextureManager::GetInstance()->TryLoad("resources/" + texture, error);
}

void GameScene::SetAnimationPreview(SceneObjectId objectId, bool playing, bool reset) {
	if (!runtimeObjects_.contains(objectId)) {
		return;
	}
	RuntimeObject& runtime = runtimeObjects_.at(objectId);
	runtime.animationPreview = playing;
	if (reset) {
		runtime.drawable->ResetAnimation();
	}
}

float GameScene::GetAnimationTime(SceneObjectId objectId) const {
	if (!runtimeObjects_.contains(objectId)) {
		return 0;
	}
	return runtimeObjects_.at(objectId).drawable->GetAnimationTime();
}

bool GameScene::IsAnimationPreviewPlaying(SceneObjectId objectId) const {
	if (!runtimeObjects_.contains(objectId)) {
		return false;
	}
	return runtimeObjects_.at(objectId).animationPreview;
}

uint64_t GameScene::GetModelPreview(const std::string& model, std::string& error) {
	if (modelPreviews_.contains(model)) {
		return modelPreviews_.at(model)->target.GetTextureHandle();
	}
	if (modelPreviews_.size() >= 24) {
		return 0;
	}
	if (!PrepareModel(model,error)) {
		return 0;
	}
	std::unique_ptr<ModelPreview> preview = std::make_unique<ModelPreview>();
	preview->target.Initialize(object3dCommon_->GetDxCommon(),object3dCommon_->GetSrvManager());
	preview->target.Resize(128,128);
	preview->object = std::make_unique<Object3d>();
	preview->object->Initialize(object3dCommon_);
	Model* loadedModel = ModelManager::GetInstance()->FindModel(model);
	preview->object->SetModel(loadedModel);
	preview->object->SetTexture("resources/white1x1.png");
	preview->object->SetEnableLighting(false);
	preview->object->SetMaterials({});
	const AABB bounds = SceneViewMath::TransformBounds(loadedModel->GetLocalBounds(),loadedModel->GetRootNode().localMatrix);
	const float diameter = (std::max)({bounds.max.x-bounds.min.x,bounds.max.y-bounds.min.y,bounds.max.z-bounds.min.z,0.001f});
	const float scale = 1.2f / diameter;
	const Vector3 center = MathUtility::Multiply(0.5f,MathUtility::Add(bounds.min,bounds.max));
	preview->world = MathUtility::MakeAffineMatrix({scale,scale,scale},{},MathUtility::Multiply(-scale,center));
	preview->world = MathUtility::Multiply(preview->world,MathUtility::MakeAffineMatrix({1,1,1},{0.2f,0.4f,0},{}));
	const uint64_t handle = preview->target.GetTextureHandle();
	modelPreviews_[model] = std::move(preview);
	return handle;
}

void GameScene::DrawModelPreviews() {
	for (std::pair<const std::string,std::unique_ptr<ModelPreview>>& entry : modelPreviews_) {
		ModelPreview& preview = *entry.second;
		if (preview.rendered) {
			continue;
		}
		const Matrix4x4 cameraWorld = MathUtility::MakeAffineMatrix({1,1,1},{},{0,0,-3});
		const Matrix4x4 projection = MathUtility::MakePerspectiveFovMatrix(0.785398163f,1,0.1f,100);
		preview.object->Update(MathUtility::Multiply(MathUtility::Inverse(cameraWorld),projection),preview.world);
		preview.target.Begin();
		object3dCommon_->DrawSettingCommon();
		preview.object->Draw();
		preview.target.End();
		preview.rendered = true;
	}
}

// AABBで絞った候補だけを、描画と同じ頂点・ワールド行列で判定する。
bool GameScene::IntersectObjectSurface(size_t objectIndex, const Vector3& origin, const Vector3& direction, float& distance) const {
	if (objectIndex >= GetObjects().size()) {
		return false;
	}
	const SceneObjectData& data = GetObjects()[objectIndex];
	if (!data.hasMeshRenderer) {
		return false;
	}
	if (!IsObjectActive(data.id)) {
		return false;
	}
	const Model* model = ModelManager::GetInstance()->FindModel(data.model);
	if (model == nullptr) {
		return false;
	}
	Matrix4x4 world{};
	if (!GetObjectWorldMatrix(data.id, world)) {
		return false;
	}
	if (data.useRootNodeMatrix) {
		world = MathUtility::Multiply(model->GetRootNode().localMatrix, world);
	}
	const std::vector<Mesh::VertexData>* vertices = &model->GetVertices();
	if (runtimeObjects_.contains(data.id)) {
		vertices = &runtimeObjects_.at(data.id).drawable->GetSurfaceVertices();
	}
	bool hit = false;
	float nearest = (std::numeric_limits<float>::max)();
	for (size_t vertexIndex = 0; vertexIndex + 2 < vertices->size(); vertexIndex += 3) {
		const Vector4& first = (*vertices)[vertexIndex].position;
		const Vector4& second = (*vertices)[vertexIndex + 1].position;
		const Vector4& third = (*vertices)[vertexIndex + 2].position;
		float candidate = 0;
		if (SceneViewMath::IntersectTriangle(origin, direction,
			MathUtility::Transform({first.x, first.y, first.z}, world),
			MathUtility::Transform({second.x, second.y, second.z}, world),
			MathUtility::Transform({third.x, third.y, third.z}, world), candidate)) {
			if (candidate < nearest) {
				nearest = candidate;
				hit = true;
			}
		}
	}
	if (hit) {
		distance = nearest;
	}
	return hit;
}
