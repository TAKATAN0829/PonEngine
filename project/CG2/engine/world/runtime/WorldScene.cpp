#include "FrameProfiler.h"
#include "runtime/WorldScene.h"
#include "WinApp.h"
#include "systems/WorldLighting.h"
#include "LogSystem.h"
#include "ModelManager.h"
#include "SceneViewMath.h"
#include "TextureManager.h"
#include "ParticleRenderer.h"
#include <unordered_set>
#include <limits>
#include <filesystem>

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// 初期化処理


void WorldScene::Initialize(const SceneContext& context) {
	motion_.Initialize();

	collision_.Initialize();

	physics_.Finalize();

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
	// Particle初期化

	particleRenderer_ = context.particleRenderer;

	particle_ = new Particle();

	particle_->Initialize(context.particleRenderer);

	particle_->SetTexture("resources/circle.png");

}

//=============================================================================================//
// 更新処理


void WorldScene::Update(Input*) {
	// ゲーム更新からだけ呼び、Pause・編集時のカメラ操作では時計を進めない。
	{
		FrameProfiler::Scope motionTime(FrameProfiler::Section::Motion);

		motion_.Update(*this, 1.0f / 60.0f);
	}

	{
		FrameProfiler::Scope physicsTime(FrameProfiler::Section::Physics);

		physics_.Update(*this,1.0f / 60.0f);

		if (physics_.GetError() != physicsStatusLogged_) {
			physicsStatusLogged_ = physics_.GetError();

			if (!physicsStatusLogged_.empty()) {
				LogSystem::Log("[Physics] " + physicsStatusLogged_ + "\n");
			}
		}
	}

	UpdateCollisions();

	// Camera更新
	UpdateCamera();

	// Object更新
	UpdateObject();
}

// 編集・一時停止中もカメラと描画は更新し、ゲーム固有の更新だけを止める。
void WorldScene::UpdateEditorFrame(Input* input, bool simulate) {
	simulationEnabled_ = simulate;

	if (simulate) {
		Update(input);
	} else {
		UpdateCamera();

		UpdateObject();
	}
}

//=============================================================================================//
// 描画処理


void WorldScene::Draw() {

	//=============================================================================================//
	// モデル描画

	const bool isolateParticles = particlePreview_.enabled && particlePreview_.isolate && !playing_;

	if (!isolateParticles) {
		object3dCommon_->DrawSettingCommon();

		for (const WorldObjectData& object : GetObjects()) {
			if (IsObjectActive(object.id)) {
				if (object.hasMeshRenderer) {
					runtimeObjects_.at(object.id).drawable->Draw();
				}
			}
		}

	}

	//=============================================================================================//
	// Particle描画

	particleRenderer_->BeginPass();

	particle_->Draw();

	for (const std::pair<const WorldObjectId, std::unique_ptr<Particle>>& entry : runtimeParticles_) {
		entry.second->Draw();
	}

	particleRenderer_->EndPass();


}

// 表示状態はEditorが所有し、閉じるボタンの結果もこの呼出中に返す。
void WorldScene::DrawEditorTools(EditorToolVisibility& visibility) {
	if (visibility.camera) {
		debugCamera_->Draw(&visibility.camera);
	}
}

//=============================================================================================//
// 終了処理


void WorldScene::Finalize() {
	motion_.Finalize();

	collision_.Finalize();

	physics_.Finalize();

	for (std::pair<const std::string,std::unique_ptr<ModelPreview>>& entry : modelPreviews_) {
		entry.second->target.Finalize();
	}

	modelPreviews_.clear();

	for (std::unique_ptr<ModelPreview>& preview : retiredPreviews_) {
		preview->target.Finalize();
	}

	retiredPreviews_.clear();

	//=============================================================================================//
	// Particle解放

	for (std::pair<const WorldObjectId, std::unique_ptr<Particle>>& entry : runtimeParticles_) {
		entry.second->Finalize();
	}

	runtimeParticles_.clear();

	particle_->Finalize();

	delete particle_;

	//=============================================================================================//
	// モデル解放

	runtimeObjects_.clear();

	//=============================================================================================//
	// Camera解放

	delete camera_;

	delete debugCamera_;
}

//=============================================================================================//
// Getter / Editor・Scene表示


uint64_t WorldScene::GetModelPreview(const std::string& model, std::string& error) {
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

bool WorldScene::PrepareModel(const std::string& model, std::string& error) {
	if (!WorldObjects::IsSupportedModel(model)) {
		error = "Unsupported model path.";

		return false;
	}

	return ModelManager::GetInstance()->TryLoadModel(model,error);
}

std::vector<ModelMaterialInfo> WorldScene::GetModelMaterials(const std::string& model) const {
	const Model* loaded = ModelManager::GetInstance()->FindModel(model);

	if (loaded == nullptr) {
		return {};
	}

	return loaded->GetMaterials();
}

std::vector<ModelAnimationInfo> WorldScene::GetModelAnimations(const std::string& model) const {
	const Model* loaded = ModelManager::GetInstance()->FindModel(model);

	if (loaded == nullptr) {
		return {};
	}

	if (loaded->GetAnimation() == nullptr) {
		return {};
	}

	return loaded->GetAnimation()->GetClips();
}

bool WorldScene::PrepareTexture(const std::string& texture, std::string& error) {
	error.clear();

	const std::filesystem::path relative(std::u8string(texture.begin(), texture.end()));

	if (texture.empty() || texture.size() > 255 || texture.find('\0') != std::string::npos) {
		error = "Invalid texture path.";

		return false;
	}

	// 文字列の形式を確認してから、resources外を指すパスを別の段階で拒否する。
	if (relative.is_absolute() || relative.has_root_name()) {
		error = "Texture must be inside resources.";

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

	if (fileError) {
		error = "Cannot resolve resources directory.";

		return false;
	}

	const std::filesystem::path resolved = std::filesystem::weakly_canonical(root / relative, fileError);

	if (fileError) {
		error = "Cannot resolve texture path.";

		return false;
	}

	const std::filesystem::path inside = resolved.lexically_relative(root);

	if (inside.empty()) {
		error = "Cannot resolve texture path.";

		return false;
	}

	if (*inside.begin() == "..") {
		error = "Texture must be inside resources.";

		return false;
	}

	return TextureManager::GetInstance()->TryLoad("resources/" + texture, error);
}

float WorldScene::GetAnimationTime(WorldObjectId objectId) const {
	if (!runtimeObjects_.contains(objectId)) {
		return 0;
	}

	return runtimeObjects_.at(objectId).drawable->GetAnimationTime();
}

bool WorldScene::IsAnimationPreviewPlaying(WorldObjectId objectId) const {
	if (!runtimeObjects_.contains(objectId)) {
		return false;
	}

	return runtimeObjects_.at(objectId).animationPreview;
}

void WorldScene::SetAnimationPreview(WorldObjectId objectId, bool playing, bool reset) {
	if (!runtimeObjects_.contains(objectId)) {
		return;
	}

	RuntimeObject& runtime = runtimeObjects_.at(objectId);

	runtime.animationPreview = playing;

	if (reset) {
		runtime.drawable->ResetAnimation();
	}
}

bool WorldScene::ReplaceObjects(std::vector<WorldObjectData> objects, std::string& error) {
	WorldObjects validator;

	if (!validator.ReplaceObjects(objects,error)) {
		return false;
	}

	for (const WorldObjectData& object : objects) {
		// Mesh Rendererを外して保存したモデルも、後から再追加できるよう準備する。
		if (!PrepareModel(object.model, error)) {
			return false;
		}

		if (object.type == WorldObjectType::Particle) {
			if (!TextureManager::GetInstance()->TryLoad(object.particle.texture, error)) {
				return false;
			}
		}

		const size_t materialCount = GetModelMaterials(object.model).size();

		for (const MaterialSettings& material : object.materials) {
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

	return WorldObjects::ReplaceObjects(std::move(objects),error);
}

// Sceneパネル内のカメラと描画サイズを公開し、画像とクリック位置を一致させる。
bool WorldScene::GetEditorViewInfo(EditorViewInfo& viewInfo) const {
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
bool WorldScene::GetEditableObjectBounds(size_t objectIndex, AABB& worldBounds) const {
	if (objectIndex >= GetObjects().size()) {
		return false;
	}

	const WorldObjectData& object = GetObjects()[objectIndex];

	if (!IsObjectActive(object.id)) {
		return false;
	}

	if (!object.hasMeshRenderer) {
		if (object.type == WorldObjectType::Model) {
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

// AABBで絞った候補だけを、描画と同じ頂点・ワールド行列で判定する。
bool WorldScene::IntersectObjectSurface(size_t objectIndex, const Vector3& origin, const Vector3& direction, float& distance) const {
	if (objectIndex >= GetObjects().size()) {
		return false;
	}

	const WorldObjectData& data = GetObjects()[objectIndex];

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

//=============================================================================================//
// Asset参照の更新

void WorldScene::RemapAssetPaths(const AssetPathChange& change) {
	TextureManager::GetInstance()->RemapAssetPaths(change);

	ModelManager::GetInstance()->RemapAssetPaths(change);

	WorldObjects::RemapAssetPaths(change);

	particleSettings_.texture = change.RemapResource(particleSettings_.texture);

	for (std::pair<const WorldObjectId, RuntimeObject>& entry : runtimeObjects_) {
		entry.second.model = change.RemapRelative(entry.second.model);
	}

	std::vector<std::string> changedKeys;

	for (const std::pair<const std::string, std::unique_ptr<ModelPreview>>& entry : modelPreviews_) {
		if (change.RemapRelative(entry.first) != entry.first) {
			changedKeys.push_back(entry.first);
		}
	}

	for (const std::string& key : changedKeys) {
		const std::string destination = change.RemapRelative(key);

		std::unordered_map<std::string, std::unique_ptr<ModelPreview>>::node_type node = modelPreviews_.extract(key);

		if (modelPreviews_.contains(destination)) {
			retiredPreviews_.push_back(std::move(modelPreviews_.at(destination)));

			modelPreviews_.erase(destination);
		}

		node.key() = destination;

		modelPreviews_.insert(std::move(node));
	}
}

//=============================================================================================//
// Setter / Scene表示・カメラ


// 表示領域の大きさを投影行列と2D描画へ同時に渡す。
void WorldScene::SetEditorViewport(uint32_t width, uint32_t height) {
	if (width == 0 || height == 0) {
		return;
	}

	// 変わっていない表示サイズではカメラの投影行列を作り直さない。
	if (viewportWidth_ == width && viewportHeight_ == height) {
		return;
	}

	viewportWidth_ = width;

	viewportHeight_ = height;

	debugCamera_->SetViewportSize(static_cast<int32_t>(width), static_cast<int32_t>(height));

	camera_->SetViewportSize(static_cast<int32_t>(width), static_cast<int32_t>(height));
}

// Scene画像の上にいるときだけEditorカメラの入力を受け付ける。
void WorldScene::SetEditorCameraInput(bool enabled) {
	debugCamera_->SetEditorInputEnabled(enabled);
}

// Editorの注視要求を、シーンが所有するカメラへ渡す。
void WorldScene::FocusEditorCamera(const Vector3& position) {
	if (debugCamera_ != nullptr) {
		debugCamera_->Focus(position);
	}
}

//=============================================================================================//
// Getter / Particle


// 表示上限だけではなく、実際に管理している粒子数をEditorへ通知する。
size_t WorldScene::GetParticleCount() const {
	if (particlePreview_.targetObject != kInvalidWorldObjectId) {
		const std::unordered_map<WorldObjectId, std::unique_ptr<Particle>>::const_iterator emitter = runtimeParticles_.find(particlePreview_.targetObject);

		if (emitter != runtimeParticles_.end()) {
			return emitter->second->GetLiveCount();
		}

		return 0;
	}

	return particle_->GetLiveCount();
}

//=============================================================================================//
// Setter / Particle操作


// プレビューの再開や設定読込時は、古い粒子を残さず作り直す。
void WorldScene::ResetParticlePreview() {
	particle_->ResetSimulation();

	for (std::pair<const WorldObjectId, std::unique_ptr<Particle>>& entry : runtimeParticles_) {
		entry.second->ResetSimulation();
	}
}

// 再生開始・停止時に粒子と発生タイマーを消し、毎回同じ状態から確認できるようにする。
void WorldScene::OnPlayStateChanged(bool playing) {
	playing_ = playing;

	physicsStatusLogged_.clear();

	motion_.Initialize();

	collision_.Initialize();

	physics_.Finalize();

	if (playing) {
		physics_.Initialize(*this);
	}

	for (std::pair<const WorldObjectId, RuntimeObject>& entry : runtimeObjects_) {
		entry.second.animationPreview = false;

		entry.second.drawable->ResetAnimation();
	}

	ResetParticlePreview();
}

//=============================================================================================//
// Getter / 表示条件

bool WorldScene::ShouldDrawGameOverlay() const {
	if (!gameView_) {
		return false;
	}

	if (!playing_) {
		if (particlePreview_.enabled) {
			if (particlePreview_.isolate) {
				return false;
			}
		}
	}

	return true;
}

bool WorldScene::CanHandleGameInput() const {
#ifdef USE_IMGUI
	if (!gameView_) {
		return false;
	}

	if (ImGui::GetIO().WantTextInput) {
		return false;
	}

	if (ImGui::IsAnyItemActive()) {
		return false;
	}

	if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
		return false;
	}

#endif
	return true;
}

//=============================================================================================//
// 実行状態の初期化

void WorldScene::InitializeSimulation() {
	physics_.Initialize(*this);
}

//=============================================================================================//
// 更新


// 接触は動作更新後に検出する。Stayを出さず、大量の同時変化もログを間引く。
void WorldScene::UpdateCollisions() {
	FrameProfiler::Scope collisionTime(FrameProfiler::Section::Collision);

	collision_.Update(*this);

	const std::vector<CollisionEvent>& events = collision_.GetEvents();

	for (size_t eventIndex = 0; eventIndex < (std::min)(events.size(),size_t{32}); ++eventIndex) {
		const CollisionEvent& event = events[eventIndex];

		std::string message = "[Collision Exit] ";

		if (event.entering) {
			message = "[Collision Enter] ";
		}

		message += std::to_string(event.first);

		const WorldObjectData* first = GetObjectDataById(event.first);

		if (first != nullptr) {
			message += " (" + first->name + ")";
		}

		message += " <-> " + std::to_string(event.second);

		const WorldObjectData* second = GetObjectDataById(event.second);

		if (second != nullptr) {
			message += " (" + second->name + ")";
		}

		LogSystem::Log(message + "\n");
	}

	if (events.size() > 32) {
		LogSystem::Log("[Collision] " + std::to_string(events.size()-32) + " additional transitions omitted this frame.\n");
	}
}

void WorldScene::UpdateCamera() {

	camera_->SetTransform(cameraTransform_);

	camera_->Update();

	debugCamera_->Update();
}

// カメラ情報だけをここで用意し、モデル・Emitterの更新へ渡す。
void WorldScene::UpdateObject() {
	Matrix4x4 cameraMatrix = debugCamera_->GetCameraMatrix();
	Matrix4x4 viewProjection = debugCamera_->GetViewProjectionMatrix();

	if (gameView_) {
		cameraMatrix = MathUtility::Inverse(camera_->GetViewMatrix());

		viewProjection = camera_->GetViewProjectionMatrix();

		CameraView cameraView{};

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
}

void WorldScene::UpdateModels(const Matrix4x4& viewProjection) {
	// 前フレームのGPU完了後、描画命令を記録する前に不要なリソースを破棄する。
	const WorldLightCollection lights = WorldLighting::Collect(*this);

	std::unordered_set<WorldObjectId> liveIds;

	for (const WorldObjectData& data : GetObjects()) {
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

		object.SetSceneLights(lights);

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

	std::unordered_map<WorldObjectId, RuntimeObject>::iterator runtimeObjectIterator = runtimeObjects_.begin();

	while (runtimeObjectIterator != runtimeObjects_.end()) {
		if (liveIds.contains(runtimeObjectIterator->first)) {
			++runtimeObjectIterator;
		} else {
			runtimeObjectIterator = runtimeObjects_.erase(runtimeObjectIterator);
		}
	}
}

void WorldScene::UpdateParticles(const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix) {
	std::unordered_set<WorldObjectId> particleIds;

	for (const WorldObjectData& data : GetObjects()) {
		if (!IsObjectActive(data.id)) {
			continue;
		}

		if (data.type != WorldObjectType::Particle) {
			continue;
		}

		particleIds.insert(data.id);

		UpdateEmitter(data, viewProjection, cameraMatrix);
	}

	std::unordered_map<WorldObjectId, std::unique_ptr<Particle>>::iterator emitterIterator = runtimeParticles_.begin();

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
		if (particlePreview_.targetObject == kInvalidWorldObjectId) {
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
		ShouldSimulateEmitter(kInvalidWorldObjectId, commonParticleEmission_ && runtimeParticles_.empty()),
		ShouldEmitEmitter(kInvalidWorldObjectId, commonParticleEmission_ && runtimeParticles_.empty()));

	particlePreview_.step = false;
}

void WorldScene::UpdateEmitter(const WorldObjectData& data, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix) {
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

void WorldScene::DrawModelPreviews() {
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

//=============================================================================================//
// Getter / Particle更新条件


bool WorldScene::IsParticlePreviewTarget(WorldObjectId objectId) const {
	if (playing_) {
		return false;
	}

	if (!particlePreview_.enabled) {
		return false;
	}

	return particlePreview_.targetObject == objectId;
}

bool WorldScene::ShouldSimulateEmitter(WorldObjectId objectId, bool gameEmissionEnabled) const {
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

bool WorldScene::ShouldEmitEmitter(WorldObjectId objectId, bool gameEmissionEnabled) const {
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

