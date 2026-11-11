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
	isGameView_ = true; // Releaseも保存したメインカメラを使う。

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

	// サムネイル描画にも、このシーンと同じ描画基盤を渡す。
	modelPreviewRenderer_.Initialize(object3dCommon_);

	//=============================================================================================//
	// Particle初期化

	particleRenderer_ = context.particleRenderer;

	particle_ = new Particle();

	particle_->Initialize(context.particleRenderer);

	particle_->SetTexture("resources/circle.png");

	commonParticleEffect_ = MakeParticleEffectDocument(particleSettings_);

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

		// 同じ物理エラーを毎フレーム出さず、内容が変わったときだけログを更新する。
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
void WorldScene::UpdateEditorFrame(Input* input, bool shouldSimulate) {
	// 編集・一時停止中の表示更新と、再生中のゲーム更新を分ける。
	isSimulationEnabled_ = shouldSimulate;

	if (shouldSimulate) {
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

	const bool shouldIsolateParticles = particlePreview_.isEnabled && particlePreview_.shouldIsolateParticles && !isPlaying_;

	if (!shouldIsolateParticles) {
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

	for (const std::pair<const WorldObjectId, std::vector<RuntimeParticleEmitter>>& entry : runtimeParticles_) {
		for (const RuntimeParticleEmitter& emitter : entry.second) {
			emitter.particle->Draw();
		}
	}

	particleRenderer_->EndPass();


}

// 表示状態はEditorが所有し、閉じるボタンの結果もこの呼出中に返す。
void WorldScene::DrawEditorTools(EditorToolVisibility& visibility) {
	// 開くよう指定された補助画面だけを共通の描画機能へ渡す。
	if (visibility.isCameraVisible) {
		debugCamera_->Draw(&visibility.isCameraVisible);
	}
}

//=============================================================================================//
// 終了処理

void WorldScene::Finalize() {
	motion_.Finalize();

	collision_.Finalize();

	physics_.Finalize();

	// シーン切替はGPUの完了後に行われるため、ここでサムネイルの描画先を解放できる。
	modelPreviewRenderer_.Finalize();
	if (isParticleEditorPreviewTargetInitialized_) {
		particleEditorPreviewTarget_.Finalize();
		isParticleEditorPreviewTargetInitialized_ = false;
	}
	for (RuntimeParticleEmitter& emitter : particleEditorPreviewEmitters_) {
		emitter.particle->Finalize();
	}
	particleEditorPreviewEmitters_.clear();

	//=============================================================================================//
	// Particle解放

	for (std::pair<const WorldObjectId, std::vector<RuntimeParticleEmitter>>& entry : runtimeParticles_) {
		for (RuntimeParticleEmitter& emitter : entry.second) {
			emitter.particle->Finalize();
		}
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

// 接触色は保存対象外の最新ゲーム更新結果から取得する。
bool WorldScene::IsColliderTouching(WorldObjectId identifier) const {
	// 指定したオブジェクトが他のColliderと接触しているか確認する。
	return collision_.IsTouching(identifier);
}

uint64_t WorldScene::GetModelPreview(const std::string& model, std::string& error) {
	// 描画先があれば再利用し、上限に達した場合は新しいモデルを読まない。
	const uint64_t cachedHandle = modelPreviewRenderer_.GetTextureHandle(model);

	if (cachedHandle != 0) {
		return cachedHandle;
	}

	if (!modelPreviewRenderer_.CanCreatePreview()) {
		return 0;
	}

	// シーンと同じモデルの検証・読込を通し、描画先の準備は専用クラスへ渡す。
	if (!PrepareModel(model, error)) {
		return 0;
	}

	Model* loadedModel = ModelManager::GetInstance()->FindModel(model);

	if (loadedModel == nullptr) {
		error = "Model is unavailable for preview.";

		return 0;
	}

	return modelPreviewRenderer_.CreatePreview(model, *loadedModel);
}

bool WorldScene::PrepareModel(const std::string& model, std::string& error) {
	// 必要なモデルを読込済みにし、失敗した理由を返す。
	if (!WorldObjects::IsSupportedModel(model)) {
		error = "Unsupported model path.";

		return false;
	}

	return ModelManager::GetInstance()->TryLoadModel(model,error);
}

std::vector<ModelMaterialInfo> WorldScene::GetModelMaterials(const std::string& model) const {
	// 指定モデルのMaterial情報を取得する。
	const Model* loaded = ModelManager::GetInstance()->FindModel(model);

	if (loaded == nullptr) {
		return {};
	}

	return loaded->GetMaterials();
}

std::vector<ModelAnimationInfo> WorldScene::GetModelAnimations(const std::string& model) const {
	// 指定モデルのアニメーション情報を取得する。
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
	// 現在のアニメーション時刻を取得する。
	if (!runtimeObjects_.contains(objectId)) {
		return 0;
	}

	return runtimeObjects_.at(objectId).drawable->GetAnimationTime();
}

bool WorldScene::IsAnimationPreviewPlaying(WorldObjectId objectId) const {
	// 対象IDのアニメーションプレビューが再生中か確認する。
	if (!runtimeObjects_.contains(objectId)) {
		return false;
	}

	return runtimeObjects_.at(objectId).isAnimationPreviewEnabled;
}

void WorldScene::SetAnimationPreview(WorldObjectId objectId, bool isPlaying, bool shouldReset) {
	// 対象モデルのプレビュー状態と再生位置を更新する。
	if (!runtimeObjects_.contains(objectId)) {
		return;
	}

	RuntimeObject& runtime = runtimeObjects_.at(objectId);

	runtime.isAnimationPreviewEnabled = isPlaying;

	if (shouldReset) {
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
			// Scene反映前に全Emitterの画像を準備し、途中失敗で実行状態だけが欠けないようにする。
			for (const ParticleEffectNode& node : object.particleEffect.nodes) {
				if (node.type == ParticleNodeType::Emitter &&
					!TextureManager::GetInstance()->TryLoad(node.emitter.texture, error)) {
					return false;
				}
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

IEditableWorld* WorldScene::GetEditableWorld() {
	// このシーン自身を編集用の窓口として公開する。
	return this;
}

// Sceneパネル内のカメラと描画サイズを公開し、画像とクリック位置を一致させる。
bool WorldScene::GetEditorViewInfo(EditorViewInfo& viewInfo) const {
	// SceneViewのカメラと表示サイズを取得する。
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

	if (object.isRootNodeMatrixUsed) {
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
	// メッシュの三角形を調べ、レイが最初に当たる表面を求める。
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

	if (data.isRootNodeMatrixUsed) {
		world = MathUtility::Multiply(model->GetRootNode().localMatrix, world);
	}

	const std::vector<Mesh::VertexData>* vertices = &model->GetVertices();

	if (runtimeObjects_.contains(data.id)) {
		vertices = &runtimeObjects_.at(data.id).drawable->GetSurfaceVertices();
	}

	bool hasHit = false;

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

				hasHit = true;
			}
		}
	}

	if (hasHit) {
		distance = nearest;
	}

	return hasHit;
}

//=============================================================================================//
// Asset参照の更新

void WorldScene::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	TextureManager::GetInstance()->RemapAssetPaths(change);

	ModelManager::GetInstance()->RemapAssetPaths(change);

	WorldObjects::RemapAssetPaths(change);

	particleSettings_.texture = change.RemapResource(particleSettings_.texture);
	for (ParticleEffectNode& node : commonParticleEffect_.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			node.emitter.texture = change.RemapResource(node.emitter.texture);
		}
	}
	for (ParticleEffectNode& node : particlePreviewEffect_.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			node.emitter.texture = change.RemapResource(node.emitter.texture);
		}
	}

	for (std::pair<const WorldObjectId, RuntimeObject>& entry : runtimeObjects_) {
		entry.second.model = change.RemapRelative(entry.second.model);
	}

	// Projectから移動後のパスで同じサムネイルを取得できるようにする。
	modelPreviewRenderer_.RemapAssetPaths(change);
}

// 新しい配置へ前の速度・接触・プレビュー・カメラ操作を持ち越さない。
void WorldScene::OnSceneContentsChanged() {
	// 新しい配置へ実行時の一時状態を持ち越さない。
	OnPlayStateChanged(false);

	particlePreview_ = {};

	isParticleBurstRequested_ = false;

	isGameView_ = false;

	// 前の操作位置を持ち越さず、Editorカメラを初期化する。
	debugCamera_->Initialize(static_cast<int32_t>(viewportWidth_), static_cast<int32_t>(viewportHeight_));

	debugCamera_->SetEditorInputEnabled(false);
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
void WorldScene::SetEditorCameraInput(bool isEnabled) {
	// SceneView上でのカメラ操作を受け付けるか切り替える。
	debugCamera_->SetEditorInputEnabled(isEnabled);
}

void WorldScene::SetEditorGameView(bool isEnabled) {
	// ゲーム用のカメラと表示を使うか切り替える。
	isGameView_ = isEnabled;
}

// Editorの注視要求を、シーンが所有するカメラへ渡す。
void WorldScene::FocusEditorCamera(const Vector3& position) {
	// Editor用カメラへ注視する位置を渡す。
	if (debugCamera_ != nullptr) {
		debugCamera_->Focus(position);
	}
}

//=============================================================================================//
// Getter / Particle

ParticleSettings* WorldScene::GetParticleSettings() {
	// 共通Particleプリセットの設定を取得する。
	return &particleSettings_;
}

// 表示上限だけではなく、実際に管理している粒子数をEditorへ通知する。
size_t WorldScene::GetParticleCount() const {
	// 現在生存しているParticleの合計数を取得する。
	if (particlePreview_.targetObject != kInvalidWorldObjectId ||
		runtimeParticles_.contains(kInvalidWorldObjectId) ||
		(IsParticlePreviewTarget(kInvalidWorldObjectId) && !particlePreviewEffect_.nodes.empty())) {
		const std::unordered_map<WorldObjectId, std::vector<RuntimeParticleEmitter>>::const_iterator emitter = runtimeParticles_.find(particlePreview_.targetObject);

		if (emitter != runtimeParticles_.end()) {
			size_t count = 0;
			// 選択Componentの表示数は、各Emitterの粒子・残像を合算する。
			for (const RuntimeParticleEmitter& runtimeEmitter : emitter->second) {
				count += runtimeEmitter.particle->GetLiveCount();
			}
			return count;
		}

		return 0;
	}

	return particle_->GetLiveCount();
}

//=============================================================================================//
// Setter / Particle操作

void WorldScene::SetParticlePreview(const ParticlePreviewState& state) {
	// 次の更新で使用するParticleのプレビュー条件を保持する。
	particlePreview_ = state;
}

void WorldScene::SetParticlePreviewViewport(uint32_t width, uint32_t height) {
	// 表示中のParticleEditorにだけ専用描画先を確保し、閉じたら描画を止める。
	particleEditorPreviewWidth_ = width;
	particleEditorPreviewHeight_ = height;
	if (width == 0 || height == 0) {
		return;
	}

	if (!isParticleEditorPreviewTargetInitialized_) {
		particleEditorPreviewTarget_.Initialize(object3dCommon_->GetDxCommon(), object3dCommon_->GetSrvManager());
		isParticleEditorPreviewTargetInitialized_ = true;
	}

	particleEditorPreviewTarget_.Resize(width, height);
}

uint64_t WorldScene::GetParticlePreviewTexture() const {
	// まだPreview領域が要求されていない場合は、空ハンドルを返す。
	if (!isParticleEditorPreviewTargetInitialized_) {
		return 0;
	}
	return particleEditorPreviewTarget_.GetTextureHandle();
}

void WorldScene::SetParticlePreviewEffect(const ParticleEffectDocument& document) {
	// 共通プリセットのEffect文書をSceneの再生処理へ渡す。
	if (!AreParticleEffectDocumentsEqual(particlePreviewEffect_, document)) {
		particlePreviewEffect_ = document;
	}
}

// プレビューの再開や設定読込時は、古い粒子を残さず作り直す。
void WorldScene::ResetParticlePreview() {
	// 生存中の粒子と発生タイマーを初期状態へ戻す。
	isParticleBurstRequested_ = false;
	pendingParticleBurstCounts_.clear();

	particle_->ResetSimulation();

	for (std::pair<const WorldObjectId, std::vector<RuntimeParticleEmitter>>& entry : runtimeParticles_) {
		for (RuntimeParticleEmitter& emitter : entry.second) {
			emitter.particle->ResetSimulation();
		}
	}
	for (RuntimeParticleEmitter& emitter : particleEditorPreviewEmitters_) {
		emitter.particle->ResetSimulation();
	}
}

void WorldScene::BurstParticles() {
	// 次回の更新で一度だけ粒子を発生させる要求を立てる。
	isParticleBurstRequested_ = true;
}

bool WorldScene::BurstParticleEffect(WorldObjectId objectId) {
	// 有効なParticleオブジェクトだけを対象にし、同Frame内の複数要求も数える。
	const WorldObjectData* object = GetObjectDataById(objectId);
	if (object == nullptr || object->type != WorldObjectType::Particle || !IsObjectActive(objectId)) {
		return false;
	}

	uint32_t& burstCount = pendingParticleBurstCounts_[objectId];
	if (burstCount < (std::numeric_limits<uint32_t>::max)()) {
		++burstCount;
	}
	return true;
}

// 再生開始・停止時に粒子と発生タイマーを消し、毎回同じ状態から確認できるようにする。
void WorldScene::OnPlayStateChanged(bool isPlaying) {
	// 物理・動作・接触・アニメーション・粒子を再生開始前の状態へ戻す。
	isPlaying_ = isPlaying;

	physicsStatusLogged_.clear();

	motion_.Initialize();

	collision_.Initialize();

	// 前の再生で保持した速度を消す。
	physics_.Finalize();

	// 再生開始時の配置と初速を物理処理へ渡す。
	if (isPlaying) {
		physics_.Initialize(*this);
	}

	for (std::pair<const WorldObjectId, RuntimeObject>& entry : runtimeObjects_) {
		entry.second.isAnimationPreviewEnabled = false;

		entry.second.drawable->ResetAnimation();
	}

	// 実行中の粒子と発生タイマーも消す。
	ResetParticlePreview();
}

//=============================================================================================//
// Getter / 表示条件

bool WorldScene::ShouldDrawGameOverlay() const {
	// 表示モードとParticleの分離表示から、ゲーム用HUDを描くか判定する。
	if (!isGameView_) {
		return false;
	}

	if (!isPlaying_) {
		if (particlePreview_.isEnabled) {
			if (particlePreview_.shouldIsolateParticles) {
				return false;
			}
		}
	}

	return true;
}

bool WorldScene::CanHandleGameInput() const {
	// GameViewの表示とUIの入力状態から、ゲームへ入力を渡すか判定する。
#ifdef USE_IMGUI
	if (!isGameView_) {
		return false;
	}

	// 入力状態を取得。
	if (ImGui::GetIO().WantTextInput) {
		return false;
	}

	// UI操作中か確認。
	if (ImGui::IsAnyItemActive()) {
		return false;
	}

	// ポップアップ状態を確認。
	if (ImGui::IsPopupOpen("", ImGuiPopupFlags_AnyPopupId)) {
		return false;
	}

#endif
	return true;
}

bool WorldScene::IsPlaying() const {
	// 編集モードとゲームの再生状態を区別する。
	return isPlaying_;
}

//=============================================================================================//
// Setter / 共通Particle

// 個別Emitterとは別のサンプル発生。必要な場面だけが明示的に有効にする。
void WorldScene::SetCommonParticleEmission(bool isEnabled) {
	// 共通Emitterの連続発生の可否を切り替える。
	isCommonParticleEmissionEnabled_ = isEnabled;
}

void WorldScene::SetCommonParticleEffect(const ParticleEffectDocument* document) {
	// 不正なEffectで実行時のEmitterを壊さず、有効な文書だけを取り込む。
	if (document == nullptr || !IsParticleEffectDocumentValid(*document)) {
		return;
	}
	if (AreParticleEffectDocumentsEqual(commonParticleEffect_, *document)) {
		return;
	}

	std::unordered_map<WorldObjectId, std::vector<RuntimeParticleEmitter>>::iterator runtime = runtimeParticles_.find(kInvalidWorldObjectId);
	if (runtime != runtimeParticles_.end()) {
		for (RuntimeParticleEmitter& emitter : runtime->second) {
			emitter.particle->Finalize();
		}
		runtimeParticles_.erase(runtime);
	}

	commonParticleEffect_ = *document;
	for (const ParticleEffectNode& node : commonParticleEffect_.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			particleSettings_ = node.emitter;
			break;
		}
	}
	particle_->ResetSimulation();
}

//=============================================================================================//
// 実行状態の初期化

void WorldScene::InitializeSimulation() {
	// 再生開始時の配置を、物理更新の基準として記録する。
	physics_.Initialize(*this);
}

//=============================================================================================//
// 更新

// 接触は動作更新後に検出する。Stayを出さず、大量の同時変化もログを間引く。
void WorldScene::UpdateCollisions() {
	// 現在の配置から衝突判定を更新し、実行用の接触状態へ反映する。
	FrameProfiler::Scope collisionTime(FrameProfiler::Section::Collision);

	collision_.Update(*this);

	const std::vector<CollisionEvent>& events = collision_.GetEvents();

	for (size_t eventIndex = 0; eventIndex < (std::min)(events.size(),size_t{32}); ++eventIndex) {
		const CollisionEvent& event = events[eventIndex];

		std::string message = "[Collision Exit] ";

		if (event.isEntering) {
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
	// Scene／Gameの表示に合わせ、使用するカメラの行列を更新する。

	camera_->SetTransform(cameraTransform_);

	camera_->Update();

	debugCamera_->Update();
}

// カメラ情報だけをここで用意し、モデル・Emitterの更新へ渡す。
void WorldScene::UpdateObject() {
	// 保存対象の設定を、モデルの行列・Material・アニメーションへ反映する。
	Matrix4x4 cameraMatrix = debugCamera_->GetCameraMatrix();
	Matrix4x4 viewProjection = debugCamera_->GetViewProjectionMatrix();

	if (isGameView_) {
		cameraMatrix = MathUtility::Inverse(camera_->GetViewMatrix());

		viewProjection = camera_->GetViewProjectionMatrix();

		CameraView cameraView{};
		const WorldObjectId mainCameraId = GetMainCameraId();

		if (mainCameraId != kInvalidWorldObjectId && GetCameraView(mainCameraId,
			static_cast<float>(viewportWidth_) / static_cast<float>(viewportHeight_), cameraView)) {
			cameraMatrix = cameraView.cameraWorld;

			viewProjection = cameraView.viewProjection;
		}
	}

	object3dCommon_->SetCameraPosition({cameraMatrix.m[3][0], cameraMatrix.m[3][1], cameraMatrix.m[3][2]});

	{
		FrameProfiler::Scope sectionTime(FrameProfiler::Section::Previews);

		// 専用の描画先へ描いてから、通常のシーン描画を続ける。
		modelPreviewRenderer_.Draw();
	}

	{
		FrameProfiler::Scope sectionTime(FrameProfiler::Section::Models);

		UpdateModels(viewProjection);
	}

	{
		FrameProfiler::Scope sectionTime(FrameProfiler::Section::Particles);

		UpdateParticles(viewProjection, cameraMatrix);
	}

	DrawParticleEditorPreview();
}

void WorldScene::DrawParticleEditorPreview() {
	// 編集用の小窓が要求されているときだけ、独立したParticle実行状態を描く。
	if (!isParticleEditorPreviewTargetInitialized_ || particleEditorPreviewWidth_ == 0 ||
		particleEditorPreviewHeight_ == 0) {
		return;
	}

	particleEditorPreviewTarget_.Begin();
	particleRenderer_->BeginPass();
	if (!isPlaying_ && particlePreview_.isEnabled) {
		for (RuntimeParticleEmitter& emitter : particleEditorPreviewEmitters_) {
			emitter.particle->Draw();
		}
	}
	particleRenderer_->EndPass();
	particleEditorPreviewTarget_.End();
}

void WorldScene::UpdateParticleEditorPreview() {
	// Scene用とは別のEmitterを専用カメラで更新し、両方の画像を同時に保つ。
	if (!isParticleEditorPreviewTargetInitialized_ || particleEditorPreviewWidth_ == 0 ||
		particleEditorPreviewHeight_ == 0 || isPlaying_) {
		return;
	}

	const Matrix4x4 cameraWorld = MathUtility::MakeAffineMatrix(
		{1, 1, 1}, {}, {particlePreview_.cameraTarget.x, particlePreview_.cameraTarget.y, particlePreview_.cameraTarget.z - 10.0f});
	const Matrix4x4 view = MathUtility::Inverse(cameraWorld);
	const float aspectRatio = static_cast<float>(particleEditorPreviewWidth_) / static_cast<float>(particleEditorPreviewHeight_);
	const Matrix4x4 projection = MathUtility::MakePerspectiveFovMatrix(0.785398163f, aspectRatio, 0.1f, 100.0f);
	const Matrix4x4 previewViewProjection = MathUtility::Multiply(view, projection);
	const Matrix4x4 identity = MathUtility::MakeIdentity4x4();

	if (!particlePreview_.isEnabled) {
		for (RuntimeParticleEmitter& emitter : particleEditorPreviewEmitters_) {
			emitter.particle->ResetSimulation();
		}
		return;
	}

	if (particlePreview_.targetObject == kInvalidWorldObjectId) {
		if (!particlePreviewEffect_.nodes.empty()) {
			UpdateParticleEffect(particlePreviewEffect_, kInvalidWorldObjectId, identity,
				particleEditorPreviewEmitters_, previewViewProjection, cameraWorld);
		}
		return;
	}

	const WorldObjectData* target = GetObjectDataById(particlePreview_.targetObject);
	if (target == nullptr || target->type != WorldObjectType::Particle || !IsObjectActive(target->id)) {
		for (RuntimeParticleEmitter& emitter : particleEditorPreviewEmitters_) {
			emitter.particle->Finalize();
		}
		particleEditorPreviewEmitters_.clear();
		return;
	}

	Matrix4x4 objectWorldMatrix{};
	if (!GetObjectWorldMatrix(target->id, objectWorldMatrix)) {
		return;
	}

	UpdateParticleEffect(target->particleEffect, target->id, objectWorldMatrix,
		particleEditorPreviewEmitters_, previewViewProjection, cameraWorld);
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

		object.SetEnableLighting(data.isLightingEnabled);

		object.SetShininess(data.shininess);

		object.SetUseRootNodeMatrix(data.isRootNodeMatrixUsed);

		object.SetSceneLights(lights);

		object.SetMaterials(data.materials);

		bool shouldAnimate = runtime.isAnimationPreviewEnabled;

		if (isPlaying_) {
			shouldAnimate = isSimulationEnabled_ && data.animation.shouldAutoplay;
		}

#ifndef USE_IMGUI
		shouldAnimate = isSimulationEnabled_ && data.animation.shouldAutoplay;

#endif
		object.UpdateAnimation(data.animation, 1.0f / 60.0f, shouldAnimate);

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

		UpdateEmitter(data, runtimeParticles_[data.id], viewProjection, cameraMatrix);
	}

	bool isCommonEffectPreview = false;
	if (IsParticlePreviewTarget(kInvalidWorldObjectId) && !particlePreviewEffect_.nodes.empty()) {
		isCommonEffectPreview = true;
		particleIds.insert(kInvalidWorldObjectId);
		const Matrix4x4 commonWorldMatrix = MathUtility::MakeIdentity4x4();
		UpdateParticleEffect(particlePreviewEffect_, kInvalidWorldObjectId, commonWorldMatrix,
			runtimeParticles_[kInvalidWorldObjectId], viewProjection, cameraMatrix);
	}

	bool isCommonParticleEffectEnabled = false;
	if (isCommonParticleEmissionEnabled_ && particleIds.empty() && !isCommonEffectPreview && !commonParticleEffect_.nodes.empty()) {
		isCommonParticleEffectEnabled = true;
		particleIds.insert(kInvalidWorldObjectId);
		const Matrix4x4 commonWorldMatrix = MathUtility::MakeIdentity4x4();
		UpdateParticleEffect(commonParticleEffect_, kInvalidWorldObjectId, commonWorldMatrix,
			runtimeParticles_[kInvalidWorldObjectId], viewProjection, cameraMatrix);
	}

	UpdateParticleEditorPreview();

	std::unordered_map<WorldObjectId, std::vector<RuntimeParticleEmitter>>::iterator emitterIterator = runtimeParticles_.begin();

	while (emitterIterator != runtimeParticles_.end()) {
		if (particleIds.contains(emitterIterator->first)) {
			++emitterIterator;
		}
		else {
			for (RuntimeParticleEmitter& emitter : emitterIterator->second) {
				emitter.particle->Finalize();
			}

			emitterIterator = runtimeParticles_.erase(emitterIterator);
		}
	}

	// 個別Emitterがあるゲームでは共通プリセットの自動発生を止め、二重発生を防ぐ。
	particle_->ApplySettings(particleSettings_);

	if (isParticleBurstRequested_) {
		if (particlePreview_.targetObject == kInvalidWorldObjectId && !isCommonEffectPreview && !isCommonParticleEffectEnabled) {
			particle_->AddParticles();
		}
	}

	isParticleBurstRequested_ = false;
	pendingParticleBurstCounts_.clear();

	if (isCommonEffectPreview || isCommonParticleEffectEnabled) {
		particle_->ResetSimulation();
	}
	if (isPlaying_) {
		if (!runtimeParticles_.empty()) {
			particle_->ResetSimulation();
		}
	}

	const bool isCommonParticleEnabled = isCommonParticleEmissionEnabled_ && particleIds.empty() && !isCommonEffectPreview && !isCommonParticleEffectEnabled;
	particle_->Update(viewProjection, cameraMatrix,
		ShouldSimulateEmitter(kInvalidWorldObjectId, isCommonParticleEnabled),
		ShouldEmitEmitter(kInvalidWorldObjectId, isCommonParticleEnabled));

	particlePreview_.isStepRequested = false;
}

void WorldScene::UpdateEmitter(const WorldObjectData& data, std::vector<RuntimeParticleEmitter>& emitters,
	const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix) {
	Matrix4x4 objectWorldMatrix{};
	if (!GetObjectWorldMatrix(data.id, objectWorldMatrix)) {
		return;
	}

	UpdateParticleEffect(data.particleEffect, data.id, objectWorldMatrix, emitters, viewProjection, cameraMatrix);
}

void WorldScene::UpdateParticleEffect(const ParticleEffectDocument& document, WorldObjectId objectId, const Matrix4x4& objectWorldMatrix,
	std::vector<RuntimeParticleEmitter>& runtimeEmitters, const Matrix4x4& viewProjection, const Matrix4x4& cameraMatrix) {
	// 各Emitterノードに専用の実行状態を割り当て、ノードツリーの設定を反映する。
	std::vector<const ParticleEffectNode*> emitterNodes;
	// Group数ではなくEmitterノードだけを実行対象にする。
	for (const ParticleEffectNode& node : document.nodes) {
		if (node.type == ParticleNodeType::Emitter) {
			emitterNodes.push_back(&node);
		}
	}

	// ノードの追加・削除・並び替えがあった場合だけGPUリソースを作り直す。
	bool hasSameNodes = runtimeEmitters.size() == emitterNodes.size();
	if (hasSameNodes) {
		for (size_t index = 0; index < emitterNodes.size(); ++index) {
			if (runtimeEmitters[index].nodeId != emitterNodes[index]->id) {
				hasSameNodes = false;
				break;
			}
		}
	}
	if (!hasSameNodes) {
		for (RuntimeParticleEmitter& emitter : runtimeEmitters) {
			emitter.particle->Finalize();
		}
		runtimeEmitters.clear();
		for (const ParticleEffectNode* node : emitterNodes) {
			RuntimeParticleEmitter emitter{};
			emitter.nodeId = node->id;
			emitter.particle = std::make_unique<Particle>();
			emitter.particle->Initialize(particleRenderer_);
			runtimeEmitters.push_back(std::move(emitter));
		}
	}

	for (size_t index = 0; index < emitterNodes.size(); ++index) {
		const ParticleEffectNode& node = *emitterNodes[index];
		Particle& emitter = *runtimeEmitters[index].particle;
		emitter.ApplySettings(node.emitter);

		// 親IDをたどってルートまで集め、下から上へローカル変換を合成する。
		std::vector<const ParticleEffectNode*> nodePath;
		const ParticleEffectNode* pathNode = &node;
		while (pathNode != nullptr) {
			nodePath.push_back(pathNode);
			if (pathNode->parentId == 0) {
				break;
			}
			pathNode = nullptr;
			for (const ParticleEffectNode& candidate : document.nodes) {
				if (candidate.id == nodePath.back()->parentId) {
					pathNode = &candidate;
					break;
				}
			}
		}

		Matrix4x4 nodeWorld = MathUtility::MakeIdentity4x4();
		// 無効な親Groupがあれば子Emitterも停止し、配置だけは保持する。
		bool areParentsEnabled = true;
		for (auto nodeIterator = nodePath.rbegin(); nodeIterator != nodePath.rend(); ++nodeIterator) {
			const ParticleEffectNode& pathElement = **nodeIterator;
			areParentsEnabled = areParentsEnabled && (pathElement.id == node.id || pathElement.isEnabled);
			const Matrix4x4 local = MathUtility::MakeAffineMatrix(pathElement.transform.scale, pathElement.transform.rotate,
				pathElement.transform.translate);
			nodeWorld = MathUtility::Multiply(local, nodeWorld);
		}
		// 行ベクトル規約に合わせ、Emitterローカル→Node階層→Scene Objectの順で合成する。
		const Matrix4x4 emitterWorld = MathUtility::Multiply(nodeWorld, objectWorldMatrix);
		emitter.SetEmitterWorldTransform(emitterWorld);
		emitter.SetEmitterPosition(MathUtility::Transform(node.emitter.emitterPosition, emitterWorld));

		uint64_t burstCount = 0;
		if (isParticleBurstRequested_ && particlePreview_.targetObject == objectId) {
			++burstCount;
		}
		const std::unordered_map<WorldObjectId, uint32_t>::const_iterator queuedBursts = pendingParticleBurstCounts_.find(objectId);
		if (queuedBursts != pendingParticleBurstCounts_.end()) {
			burstCount += queuedBursts->second;
		}
		if (burstCount > 0 && areParentsEnabled && node.isEnabled) {
			// 有効なGroupとEmitterだけ、要求された回数だけ発生させる。
			for (uint64_t burstIndex = 0; burstIndex < burstCount; ++burstIndex) {
				emitter.AddParticles();
			}
		}

		const bool shouldSimulate = areParentsEnabled && ShouldSimulateEmitter(objectId, true);
		const bool shouldEmit = areParentsEnabled && node.isEnabled && ShouldEmitEmitter(objectId, true);
		emitter.Update(viewProjection, cameraMatrix, shouldSimulate, shouldEmit);
	}
}

bool WorldScene::IsParticlePreviewTarget(WorldObjectId objectId) const {
	// 現在のプレビューが指定したEmitterを対象にしているか確認する。
	if (isPlaying_) {
		return false;
	}

	if (!particlePreview_.isEnabled) {
		return false;
	}

	return particlePreview_.targetObject == objectId;
}

bool WorldScene::ShouldSimulateEmitter(WorldObjectId objectId, bool isGameEmissionEnabled) const {
	// 再生とプレビューの条件から、粒子の時間を進めるか判定する。
	if (isSimulationEnabled_) {
		if (isGameEmissionEnabled) {
			return true;
		}
	}

	if (IsParticlePreviewTarget(objectId)) {
		return !particlePreview_.isPaused || particlePreview_.isStepRequested;
	}

	return false;
}

bool WorldScene::ShouldEmitEmitter(WorldObjectId objectId, bool isGameEmissionEnabled) const {
	// 再生とプレビューの条件から、粒子を新たに発生させるか判定する。
	if (isSimulationEnabled_) {
		if (isGameEmissionEnabled) {
			return true;
		}
	}

	if (IsParticlePreviewTarget(objectId)) {
		return particlePreview_.isAutomaticEmissionEnabled;
	}

	return false;
}
