#include "FrameProfiler.h"
#include "Engine.h"
#include "DirectXTex/DirectXTex.h"
#include "GameScene.h"
#include "LogSystem.h"
#include "SceneRenderTarget.h"
#include "DebugCamera.h"
#include "../project/CG2/editor/EditorSession.h"
#include "../project/CG2/editor/panels/InspectorPanel.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>

// 実GPUでEditorを描画しながら、高頻度の発生・Pause・Burst・Resetを試験する。
class GraphicsTestScene : public GameScene {
public:
	void Initialize(const SceneContext& context) override {
		GameScene::Initialize(context);
		previewGraphics_ = context.object3dCommon->GetDxCommon();
		VerifyAnimatedAsset();
		VerifyMessageQueue();
		resizeTarget_.Initialize(context.object3dCommon->GetDxCommon(), context.object3dCommon->GetSrvManager());
		originalHandle_ = resizeTarget_.GetTextureHandle();
		hierarchyParentId_ = CreateObject(SceneObjectType::Empty);
		lightChildId_ = CreateObject(SceneObjectType::PointLight);
		emitterId_ = CreateObject(SceneObjectType::Particle);
		CreateObject(SceneObjectType::Particle); // 複数EmitterのGPUリソースが共存できることも確認する。
		modelChildId_ = AddObject("axis.obj");
		std::string modelError;
		if (GetModelPreview("teapot.obj",modelError) == 0) {
			valid_ = false;
		}
		if (PlaceModel("teapot.obj",0,{4,0,0},modelError) == 0) {
			valid_ = false;
		}
		const size_t beforeInvalid = GetEditableObjectCount();
		std::ofstream brokenModel("resources/broken-preview.obj");
		brokenModel << "not a valid mesh";
		brokenModel.close();
		if (PlaceModel("broken-preview.obj",0,{},modelError) != 0) {
			valid_ = false;
		}
		if (GetEditableObjectCount() != beforeInvalid) {
			valid_ = false;
		}

		CreateObject(SceneObjectType::Camera);
		std::string hierarchyError;
		if (!SetObjectParent(emitterId_, hierarchyParentId_, hierarchyError)) {
			valid_ = false;
		}
		if (!SetObjectParent(lightChildId_, hierarchyParentId_, hierarchyError)) {
			valid_ = false;
		}
		if (!SetObjectParent(modelChildId_, hierarchyParentId_, hierarchyError)) {
			valid_ = false;
		}
		for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = GetEditableObject(objectIndex);
			if (object.id == hierarchyParentId_) {
				object.transform->translate.x = 2;
			}
		}

	}
	void Finalize() override {
		resizeTarget_.Finalize();
		GameScene::Finalize();
	}
	// 補助画面は初期状態で閉じ、必要なときだけ開閉できることを確認する。
	void DrawEditorTools(EditorToolVisibility& visibility) override {
		if (frameNumber_ == 8) {
			visibility.particleEditor = true;
			visibility.worldSettings = true;
			visibility.camera = true;
			visibility.performance = true;
		}
		if (frameNumber_ == 18) {
			visibility = {};
		}
		GameScene::DrawEditorTools(visibility);
	}
	void UpdateEditorFrame(Input* input, bool simulate) override {
		(void)simulate;
		++frameNumber_;
		if (frameNumber_ == 3) {
			VerifyMouseCamera();
		}
		if (frameNumber_ == 1) {
			VerifySurfaceGeometry();
			VerifyPhysicsSimulation();
			VerifyColliderSimulation();
			VerifyMotionSimulation();
		}
		VerifyAnimationPlayback();
		// 毎フレーム異なる寸法へ作り直しても、画像ハンドルが変わらずBarrier違反が出ないことを確認する。
		resizeTarget_.Resize(128 + frameNumber_ % 4 * 64, 128 + frameNumber_ % 3 * 48);
		resizeTarget_.Begin();
		resizeTarget_.End();
		if (resizeTarget_.GetTextureHandle() != originalHandle_) {
			valid_ = false;
		}
		ParticleSettings* settings = GetParticleSettings();
		if (frameNumber_ >= 65) {
			for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
				const EditableObjectView object = GetEditableObject(objectIndex);
				if (object.id == emitterId_) {
					settings = object.particleSettings;
				}
			}
		}
		settings->spawnCount = 100;
		settings->interval = 0.01f;
		settings->lifetimeMinimum = 0.04f;
		settings->lifetimeMaximum = 0.05f;
		ParticlePreviewState preview;
		preview.enabled = true;
		if (frameNumber_ >= 65) {
			preview.targetObject = emitterId_;
		}
		preview.isolate = true;
		if (frameNumber_ >= 30 && frameNumber_ < 50) {
			preview.paused = true;
		}
		if (frameNumber_ == 45) {
			ResetParticlePreview();
		}
		if (frameNumber_ >= 50) {
			preview.automaticEmission = false;
		}
		if (frameNumber_ == 50 || frameNumber_ == 65) {
			BurstParticles();
		}
		SetParticlePreview(preview);
		if (frameNumber_ == 75 || frameNumber_ == 80) {
			for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
				const EditableObjectView object = GetEditableObject(objectIndex);
				if (object.id == hierarchyParentId_) {
					*object.active = frameNumber_ == 80;
				}
			}
		}
		if (frameNumber_ >= 80) {
			SetEditorGameView(true);
		}
		GameScene::UpdateEditorFrame(input, false);
		if (frameNumber_ == 25) {
			if (std::abs(GetObjectWorldPosition(lightChildId_).x - 2) > 0.001f) {
				valid_ = false;
			}
			if (std::abs(GetObjectWorldPosition(emitterId_).x - 2) > 0.001f) {
				valid_ = false;
			}
		}
		if (frameNumber_ == 76 || frameNumber_ == 81) {
			for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
				if (GetEditableObject(objectIndex).id == modelChildId_) {
					AABB bounds{};
					if (GetEditableObjectBounds(objectIndex, bounds) != (frameNumber_ == 81)) {
						valid_ = false;
					}
				}
			}
		}

		if (frameNumber_ == 25) {
			const FrameProfiler::Sample& sample = FrameProfiler::GetLatest();
			if (sample.number == 0 || sample.drawCalls == 0 || sample.uiDrawCalls == 0 || sample.frameMilliseconds <= 0) {
				valid_ = false;
			}
			if (!sample.gpuTimingValid) {
				throw std::runtime_error("GPU timestamps were not collected after fence completion.");
			}
			if (!std::isfinite(sample.gpuMilliseconds) || sample.gpuMilliseconds < 0) {
				throw std::runtime_error("GPU timestamp duration is invalid.");
			}
		}
		const size_t liveCount = GetParticleCount();
		if (frameNumber_ == 10) {
			ImGui::SetWindowFocus("Particle Editor");
		}
		if (frameNumber_ == 20) {
			ImGui::SetWindowFocus("Inspector");
		}
		if (frameNumber_ == 5 || frameNumber_ == 25) {
			VerifyCompactLayout();
		}
		if (frameNumber_ == 12) {
			const char* tools[] = {"Particle Editor", "World Settings", "Camera", "Performance"};
			for (const char* toolName : tools) {
				const ImGuiWindow* window = ImGui::FindWindowByName(toolName);
				if (window == nullptr) {
					valid_ = false;
				} else {
					if (!window->Active) {
						valid_ = false;
					}
				}
			}
		}
		if (liveCount > 100) {
			valid_ = false;
		}
		if (frameNumber_ > 5 && liveCount > 0) {
			sawParticles_ = true;
		}
		if (frameNumber_ == 32) {
			pausedCount_ = liveCount;
		}
		if (frameNumber_ == 40 && liveCount != pausedCount_) {
			valid_ = false;
		}
		if (frameNumber_ == 46 && liveCount != 0) {
			valid_ = false;
		}
		if (frameNumber_ == 51 && liveCount == 0) {
			valid_ = false;
		}
		if (frameNumber_ == 66 && liveCount == 0) {
			valid_ = false;
		}
		if (frameNumber_ == 72) {
			RemoveObject(emitterId_);
		}
		if (frameNumber_ == 73 && liveCount != 0) {
			valid_ = false;
		}
		if (frameNumber_ == 60 && liveCount != 0) {
			valid_ = false;
		}
	}
	void CapturePreview() const {
		DirectX::ScratchImage preview;
		if (FAILED(DirectX::CaptureTexture(previewGraphics_->GetCommandQueue(),previewGraphics_->GetPresentedBuffer(),false,preview,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_PRESENT))) {
			throw std::runtime_error("Editor preview capture failed.");
		}
		if (FAILED(DirectX::SaveToWICFile(*preview.GetImage(0,0,0),DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),L"editor-ui.png"))) {
			throw std::runtime_error("Editor preview PNG save failed.");
		}
	}
	bool Passed() const { return valid_ && sawParticles_ && frameNumber_ >= 80; }
private:
	// 本番の更新経路で落下・接地・Pause・停止時の復元を確認する。
	void VerifyPhysicsSimulation() {
		const std::vector<SceneObjectData> original = CaptureObjects();
		SceneObjectData floor;
		floor.id = 900100;
		floor.collider.present = true;
		floor.collider.size = {20,1,20};
		floor.transform.translate.y = -0.5f;
		SceneObjectData ball;
		ball.id = 900101;
		ball.collider.present = true;
		ball.collider.shape = ColliderShape::Sphere;
		ball.body.present = true;
		ball.transform.translate.y = 3;
		std::string error;
		if (!ReplaceObjects({floor,ball},error)) {
			throw std::runtime_error(error);
		}
		EditorSession session;
		session.Initialize(*this);
		if (!session.BeginPlay(*this,error)) {
			throw std::runtime_error(error);
		}
		OnPlayStateChanged(true);
		for (size_t frame = 0; frame < 180; ++frame) {
			GameScene::UpdateEditorFrame(nullptr,true);
		}
		if (std::abs(GetObjectWorldPosition(ball.id).y-0.5f) > 0.002f || !IsColliderTouching(ball.id)) {
			throw std::runtime_error("GameScene gravity did not land body on floor.");
		}
		const float height = GetObjectWorldPosition(ball.id).y;
		session.TogglePause();
		GameScene::UpdateEditorFrame(nullptr,session.ShouldSimulate());
		if (GetObjectWorldPosition(ball.id).y != height) {
			throw std::runtime_error("Physics advanced during Pause.");
		}
		if (!session.Stop(*this,error)) {
			throw std::runtime_error(error);
		}
		OnPlayStateChanged(false);
		if (GetObjectWorldPosition(ball.id).y != 3 || IsColliderTouching(ball.id)) {
			throw std::runtime_error("Physics Stop did not restore position and clear contacts.");
		}
		if (!ReplaceObjects(original,error)) {
			throw std::runtime_error(error);
		}
	}
	// 本番のGameScene経路で接触・Pause・Console通知・リセットを確認する。
	void VerifyColliderSimulation() {
		const std::vector<SceneObjectData> original = CaptureObjects();
		SceneObjectData first;
		first.id = 900001;
		first.collider.present = true;
		SceneObjectData second = first;
		second.id = 900002;
		second.collider.shape = ColliderShape::Sphere;
		second.transform.translate.x = 0.5f;
		std::string error;
		if (!ReplaceObjects({first,second},error)) {
			throw std::runtime_error(error);
		}
		OnPlayStateChanged(true);
		GameScene::UpdateEditorFrame(nullptr,true);
		if (!IsColliderTouching(first.id) || !IsColliderTouching(second.id)) {
			throw std::runtime_error("GameScene collision did not enter.");
		}
		const size_t enterMessages = CountCollisionMessages("[Collision Enter] 900001");
		if (enterMessages == 0) {
			throw std::runtime_error("Collision Enter was not logged.");
		}
		GameScene::UpdateEditorFrame(nullptr,true);
		if (CountCollisionMessages("[Collision Enter] 900001") != enterMessages) {
			throw std::runtime_error("Collision Stay repeated Enter logs.");
		}
		GetEditableObject(1).collider->enabled = false;
		GameScene::UpdateEditorFrame(nullptr,false);
		if (!IsColliderTouching(first.id)) {
			throw std::runtime_error("Paused collision changed contacts.");
		}
		GameScene::UpdateEditorFrame(nullptr,true);
		if (IsColliderTouching(first.id) || CountCollisionMessages("[Collision Exit] 900001") == 0) {
			throw std::runtime_error("Collision Exit was not detected or logged.");
		}
		OnPlayStateChanged(false);
		if (IsColliderTouching(first.id)) {
			throw std::runtime_error("Stop retained collision contacts.");
		}
		if (!ReplaceObjects(original,error)) {
			throw std::runtime_error(error);
		}
	}
	static size_t CountCollisionMessages(const std::string& prefix) {
		size_t count = 0;
		for (const std::string& message : LogSystem::GetRecentMessages()) {
			if (message.find(prefix) != std::string::npos) {
				++count;
			}
		}
		return count;
	}
	// 本番のGameScene経路でPlay・Pause・Step・StopとInspector描画を確認する。
	void VerifyMotionSimulation() {
		const std::vector<SceneObjectData> original = CaptureObjects();
		SceneObjectData moving;
		moving.id = 999999;
		moving.motion.hasRotation = true;
		moving.motion.hasMovement = true;
		moving.collider.present = true;
		moving.collider.shape = ColliderShape::Sphere;
		moving.body.present = true;
		moving.body.mode = BodyMode::Kinematic;
		std::string error;
		if (!ReplaceObjects({moving}, error)) {
			throw std::runtime_error(error);
		}
		EditorSession session;
		session.Initialize(*this);
		if (!session.BeginPlay(*this, error)) {
			throw std::runtime_error(error);
		}
		OnPlayStateChanged(true);
		GameScene::UpdateEditorFrame(nullptr, session.ShouldSimulate());
		const TransformData advanced = GetObjectData(0)->transform;
		if (advanced.translate.y <= 0 || advanced.rotate.y <= 0) {
			throw std::runtime_error("GameScene did not run motion components.");
		}
		InspectorPanel::Draw(GetEditableObject(0), nullptr, this);
		session.TogglePause();
		GameScene::UpdateEditorFrame(nullptr, session.ShouldSimulate());
		if (GetObjectData(0)->transform.translate.y != advanced.translate.y || GetObjectData(0)->transform.rotate.y != advanced.rotate.y) {
			throw std::runtime_error("Paused motion kept advancing.");
		}
		session.RequestStep();
		GameScene::UpdateEditorFrame(nullptr, session.ShouldSimulate());
		if (GetObjectData(0)->transform.translate.y <= advanced.translate.y) {
			throw std::runtime_error("Step did not advance motion.");
		}
		if (!session.Stop(*this, error)) {
			throw std::runtime_error(error);
		}
		OnPlayStateChanged(false);
		if (GetObjectData(0)->transform.translate.y != 0 || GetObjectData(0)->transform.rotate.y != 0) {
			throw std::runtime_error("Motion Stop did not restore editing transform.");
		}
		if (!ReplaceObjects(original, error)) {
			throw std::runtime_error(error);
		}
	}
	// OS入力は送らず、ImGuiのマウス状態を一時的に差し替えてカメラ単体を確認する。
	void VerifySurfaceGeometry() {
		const std::vector<SceneObjectData> original = CaptureObjects();
		std::string error;
		SceneObjectData triangle;
		triangle.id = 98765;
		triangle.model = "fixtures/animated-skin.gltf";
		triangle.useRootNodeMatrix = false;
		triangle.transform.translate = {0,0,5};
		if (!ReplaceObjects({triangle}, error)) {
			throw std::runtime_error(error);
		}
		GameScene::UpdateEditorFrame(nullptr, false);
		float distance = 0;
		// Fixtureのメッシュはx反転後、原点から左上へ伸びる三角形。
		if (!IntersectObjectSurface(0, {-0.1f,0.1f,0}, {0,0,1}, distance)) {
			valid_ = false;
		}
		if (std::abs(distance - 5) > 0.001f) {
			valid_ = false;
		}
		if (!ReplaceObjects(original, error)) {
			throw std::runtime_error(error);
		}
	}
	void VerifyMouseCamera() {
		ImGuiIO& inputState = ImGui::GetIO();
		const ImVec2 savedDelta = inputState.MouseDelta;
		const float savedWheel = inputState.MouseWheel;
		const bool savedRight = inputState.MouseDown[ImGuiMouseButton_Right];
		const bool savedMiddle = inputState.MouseDown[ImGuiMouseButton_Middle];
		const bool savedTextInput = inputState.WantTextInput;
		DebugCamera camera;
		camera.Initialize(800, 600);
		inputState.WantTextInput = false;
		inputState.MouseDown[ImGuiMouseButton_Right] = false;
		inputState.MouseDown[ImGuiMouseButton_Middle] = false;
		inputState.MouseDelta = {};
		inputState.MouseWheel = 1;
		camera.SetEditorInputEnabled(false);
		camera.Update();
		if (camera.GetTransform().translate.z != -30) {
			valid_ = false;
		}
		camera.SetEditorInputEnabled(true);
		camera.Update();
		if (std::abs(camera.GetTransform().translate.z + 28) > 0.001f) {
			valid_ = false;
		}
		inputState.MouseWheel = 0;
		inputState.MouseDown[ImGuiMouseButton_Middle] = true;
		inputState.MouseDelta = {10, 5};
		camera.Update();
		if (std::abs(camera.GetTransform().translate.x + 0.2f) > 0.001f || std::abs(camera.GetTransform().translate.y - 0.1f) > 0.001f) {
			valid_ = false;
		}
		inputState.MouseDown[ImGuiMouseButton_Middle] = false;
		inputState.MouseDown[ImGuiMouseButton_Right] = true;
		camera.Update();
		if (std::abs(camera.GetTransform().rotate.x - 0.025f) > 0.001f || std::abs(camera.GetTransform().rotate.y - 0.05f) > 0.001f) {
			valid_ = false;
		}
		const TransformData beforeTextInput = camera.GetTransform();
		inputState.WantTextInput = true;
		inputState.MouseWheel = 2;
		camera.Update();
		if (camera.GetTransform().translate.z != beforeTextInput.translate.z || camera.GetTransform().rotate.y != beforeTextInput.rotate.y) {
			valid_ = false;
		}
		inputState.MouseDelta = savedDelta;
		inputState.MouseWheel = savedWheel;
		inputState.MouseDown[ImGuiMouseButton_Right] = savedRight;
		inputState.MouseDown[ImGuiMouseButton_Middle] = savedMiddle;
		inputState.WantTextInput = savedTextInput;
	}

	// 自作の最小glTFで、ボーン変形・補間・ループ・端のクランプを確認する。
	void VerifyAnimatedAsset() {
		std::string error;
		const std::string path = "fixtures/animated-skin.gltf";
		if (!PrepareModel(path, error)) {
			throw std::runtime_error(error);
		}
		const std::vector<ModelMaterialInfo> materials = GetModelMaterials(path);
		if (materials.size() < 2 || GetModelAnimations(path).size() != 3) {
			throw std::runtime_error("Material slots or animation clip were lost during import.");
		}
		ModelAnimation animation;
		animation.Initialize("resources/" + path);
		std::vector<Mesh::VertexData> earlier;
		std::vector<Mesh::VertexData> later;
		animation.Update(0, 0.25f, false, earlier);
		animation.Update(0, 0.75f, false, later);
		if (earlier.size() != 6 || later.size() != 6) {
			throw std::runtime_error("Animation changed vertex count.");
		}
		if (std::abs(later[0].position.x - earlier[0].position.x + 0.5f) > 0.001f) {
			throw std::runtime_error("Bone translation interpolation is incorrect.");
		}
		animation.Update(0, 1.25f, true, later);
		if (std::abs(later[0].position.x - earlier[0].position.x) > 0.001f) {
			throw std::runtime_error("Animation loop did not wrap.");
		}
		animation.Update(0, 5, false, later);
		animation.Update(0, 1, false, earlier);
		if (std::abs(later[0].position.x - earlier[0].position.x) > 0.001f) {
			throw std::runtime_error("Non-looping animation did not clamp.");
		}
		animation.Update(-1, 0, false, earlier);
		animation.Update(1, 0.5f, false, later);
		if (std::abs(later[0].position.x - earlier[0].position.y) > 0.001f || std::abs(later[0].position.y + earlier[0].position.x) > 0.001f) {
			throw std::runtime_error("Quaternion rotation interpolation is incorrect.");
		}
		animation.Update(2, 1, false, later);
		if (std::abs(later[0].position.x - 2 * earlier[0].position.x) > 0.001f) {
			throw std::runtime_error("Scale animation is incorrect.");
		}
		animatedFirstId_ = PlaceModel(path, 0, {-3, 0, 0}, error);
		animatedSecondId_ = PlaceModel(path, 0, {-5, 0, 0}, error);
		if (animatedFirstId_ == 0 || animatedSecondId_ == 0) {
			throw std::runtime_error("Animated model placement failed.");
		}
		for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
			EditableObjectView object = GetEditableObject(objectIndex);
			if (object.id == animatedFirstId_) {
				object.materials->push_back({0, "white1x1.png", {0.2f, 0.5f, 1, 1}, 60, true});
			}
			if (object.id == animatedSecondId_) {
				object.animation->speed = 2;
			}
		}
		if (GetModelPreview(path, error) == 0) {
			throw std::runtime_error("Animated model preview failed.");
		}
		// 未キャッシュのモデルをComponentなしで読み込んでも、後から安全に追加できる。
		std::filesystem::copy_file("resources/fixtures/animated-skin.gltf", "resources/fixtures/disabled.gltf", std::filesystem::copy_options::overwrite_existing);
		std::vector<SceneObjectData> snapshot = CaptureObjects();
		SceneObjectData disabled;
		disabled.id = animatedSecondId_ + 1;
		disabled.model = "fixtures/disabled.gltf";
		disabled.hasMeshRenderer = false;
		snapshot.push_back(disabled);
		if (!ReplaceObjects(std::move(snapshot), error) || GetModelMaterials(disabled.model).empty()) {
			throw std::runtime_error("Disabled Mesh Renderer did not prepare its model.");
		}
		*GetEditableObject(GetEditableObjectCount() - 1).hasMeshRenderer = true;
	}

	void VerifyAnimationPlayback() {
		if (frameNumber_ == 2) {
			SetAnimationPreview(animatedFirstId_, true, true);
			SetAnimationPreview(animatedSecondId_, true, true);
		}
		if (frameNumber_ == 20) {
			pausedAnimationTime_ = GetAnimationTime(animatedFirstId_);
			if (pausedAnimationTime_ <= 0 || std::abs(GetAnimationTime(animatedSecondId_) - 2 * pausedAnimationTime_) > 0.001f) {
				throw std::runtime_error("Animated instances share clocks or ignore speed.");
			}
			SetAnimationPreview(animatedFirstId_, false, false);
		}
		if (frameNumber_ == 25) {
			if (GetAnimationTime(animatedFirstId_) != pausedAnimationTime_) {
				throw std::runtime_error("Paused animation kept advancing.");
			}
		}
	}

	// OSの画面は操作せず、自プロセス内のタブ構成と配色を確認する。
	void VerifyCompactLayout() {
		const char* coreWindows[] = {"Hierarchy", "Scene Viewport", "Inspector", "Project", "Console"};
		for (const char* windowName : coreWindows) {
			const ImGuiWindow* window = ImGui::FindWindowByName(windowName);
			if (window == nullptr) {
				valid_ = false;
			} else {
				if (window->DockId == 0) {
					valid_ = false;
				}
			}
		}
		const char* optionalWindows[] = {"Toolbar", "Particle Editor", "World Settings", "Camera", "Performance"};
		for (const char* windowName : optionalWindows) {
			const ImGuiWindow* window = ImGui::FindWindowByName(windowName);
			if (window != nullptr) {
				if (window->Active) {
					valid_ = false;
				}
			}
		}
		ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Scene Viewport");
		if (viewportWindow != nullptr) {
			ImGuiTabBar* tabBar = ImGui::GetCurrentContext()->TabBars.GetByKey(viewportWindow->GetID("SceneViewTabs"));
			if (tabBar == nullptr) {
				valid_ = false;
			} else {
				if (tabBar->Tabs.Size != 2) {
					valid_ = false;
				} else {
					if (std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[0])) != "SceneView") {
						valid_ = false;
					}
					if (std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[1])) != "GameView") {
						valid_ = false;
					}
				}
			}
		}
		if (ImGui::GetStyle().Colors[ImGuiCol_TabSelectedOverline].w != 0) {
			valid_ = false;
		}
		if (ImGui::GetStyle().Colors[ImGuiCol_TabSelected].x != ImGui::GetStyle().Colors[ImGuiCol_Tab].x) {
			valid_ = false;
		}
		if (std::abs(ImGui::GetStyle().FontScaleMain - 0.85f) > 0.001f) {
			valid_ = false;
		}
		if (!ImGui::GetFont()->IsGlyphInFont(0x7C92)) {
			valid_ = false;
		}
	}

	// テスト用メッセージはこのスレッドだけに送り、他アプリの入力は操作しない。
	void VerifyMessageQueue() {
		constexpr UINT kTestMessage = WM_APP + 37;
		MSG message{};
		PeekMessage(&message, nullptr, 0, 0, PM_NOREMOVE);
		for (uint32_t messageIndex = 0; messageIndex < 128; ++messageIndex) {
			if (!PostThreadMessage(GetCurrentThreadId(), kTestMessage, messageIndex, 0)) {
				valid_ = false;
			}
		}
		WinApp messagePump;
		if (messagePump.ProcessMessage()) {
			valid_ = false;
		}
		if (PeekMessage(&message, nullptr, kTestMessage, kTestMessage, PM_NOREMOVE)) {
			valid_ = false;
		}
	}

	DirectXCommon* previewGraphics_ = nullptr; // テスト用。Engineの寿命中だけ借用する。
	SceneObjectId hierarchyParentId_ = kInvalidSceneObjectId;
	SceneObjectId lightChildId_ = kInvalidSceneObjectId;
	SceneObjectId modelChildId_ = kInvalidSceneObjectId;
	SceneObjectId emitterId_ = kInvalidSceneObjectId;
	SceneObjectId animatedFirstId_ = 0;
	SceneObjectId animatedSecondId_ = 0;
	float pausedAnimationTime_ = 0;
	uint32_t frameNumber_ = 0;
	size_t pausedCount_ = 0;
	bool sawParticles_ = false;
	bool valid_ = true;
	SceneRenderTarget resizeTarget_;
	uint64_t originalHandle_ = 0;
};

// 操作対象は試験用プロセス自身だけで、ユーザーのシーンやプリセットは保存しない。
int main() {
	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
	if (FAILED(comResult)) {
		return 1;
	}
	LogSystem::Initialize();
	try {
		Engine engine;
		GraphicsTestScene* scene = new GraphicsTestScene();
		engine.Initialize(scene);
		engine.Run(90);
		scene->CapturePreview();
		const bool passed = scene->Passed() && !engine.HasGraphicsErrors();
		engine.Finalize();
		LogSystem::Finalize();
		CoUninitialize();
		if (!passed) {
			std::cerr << "GPU/editor preview smoke test failed\n";
			return 1;
		}
		std::cout << "GPU/editor rendering and particle preview tests passed\n";
		return 0;
	} catch (const std::exception& exception) {
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
