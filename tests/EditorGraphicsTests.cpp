#include "FrameProfiler.h"
#include "Engine.h"
#include "LightingGraphicsTests.h"
#include "DirectXTex/DirectXTex.h"
#include "GameScene.h"
#include "LogSystem.h"
#include "SceneRenderTarget.h"
#include "serialization/SceneFile.h"
#include "DebugCamera.h"
#include "TextureManager.h"
#include "ModelManager.h"
#include "../project/CG2/editor/EditorSession.h"
#include "../project/CG2/editor/panels/inspector/InspectorPanel.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <array>

void RunSceneViewInteractionTests();
void VerifyPrimitiveAssets(GameScene& scene);

// 実GPUでEditorを描画しながら、高頻度の発生・Pause・Burst・Resetを試験する。
class GraphicsTestScene : public GameScene {
public:
	void Initialize(const SceneContext& context) override {
		GameScene::Initialize(context);
		previewGraphics_ = context.object3dCommon->GetDxCommon();
		InitializeBlendSamples(context);
		InitializeSoftSamples(context);
		lightingTests_.Initialize(context);
		VerifyPrimitiveAssets(*this);
		VerifyAnimatedAsset();
		VerifyMessageQueue();
		resizeTarget_.Initialize(context.object3dCommon->GetDxCommon(), context.object3dCommon->GetSrvManager());
		originalHandle_ = resizeTarget_.GetTextureHandle();
		hierarchyParentId_ = CreateObject(WorldObjectType::Empty);
		lightChildId_ = CreateObject(WorldObjectType::PointLight);
		emitterId_ = CreateObject(WorldObjectType::Particle);
		CreateObject(WorldObjectType::Particle); // 複数EmitterのGPUリソースが共存できることも確認する。
		modelChildId_ = AddObject("axis.obj");
		std::string modelError;
		if (GetModelPreview("teapot.obj",modelError) == 0) {
			FailTest(__LINE__);
		}
		if (PlaceModel("teapot.obj",0,{4,0,0},modelError) == 0) {
			FailTest(__LINE__);
		}
		const size_t beforeInvalid = GetEditableObjectCount();
		std::ofstream brokenModel("resources/broken-preview.obj");
		brokenModel << "not a valid mesh";
		brokenModel.close();
		if (PlaceModel("broken-preview.obj",0,{},modelError) != 0) {
			FailTest(__LINE__);
		}
		if (GetEditableObjectCount() != beforeInvalid) {
			FailTest(__LINE__);
		}

		CreateObject(WorldObjectType::Camera);
		std::string hierarchyError;
		if (!SetObjectParent(emitterId_, hierarchyParentId_, hierarchyError)) {
			FailTest(__LINE__);
		}
		if (!SetObjectParent(lightChildId_, hierarchyParentId_, hierarchyError)) {
			FailTest(__LINE__);
		}
		if (!SetObjectParent(modelChildId_, hierarchyParentId_, hierarchyError)) {
			FailTest(__LINE__);
		}
		for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
			const EditableObjectView object = GetEditableObject(objectIndex);
			if (object.id == hierarchyParentId_) {
				object.transform->translate.x = 2;
			}
		}

	}
	void Finalize() override {
		lightingTests_.Finalize();
		for (BlendSample& sample : softSamples_) {
			sample.particle.Finalize();
			sample.target.Finalize();
		}
		softPlane_.reset();
		for (BlendSample& sample : blendSamples_) {
			sample.particle.Finalize();
			sample.target.Finalize();
		}
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
		DrawBlendSamples();
		DrawSoftSamples();
		lightingTests_.Draw();
		if (resizeTarget_.GetTextureHandle() != originalHandle_) {
			FailTest(__LINE__);
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
				FailTest(__LINE__);
			}
			if (std::abs(GetObjectWorldPosition(emitterId_).x - 2) > 0.001f) {
				FailTest(__LINE__);
			}
		}
		if (frameNumber_ == 76 || frameNumber_ == 81) {
			for (size_t objectIndex = 0; objectIndex < GetEditableObjectCount(); ++objectIndex) {
				if (GetEditableObject(objectIndex).id == modelChildId_) {
					AABB bounds{};
					if (GetEditableObjectBounds(objectIndex, bounds) != (frameNumber_ == 81)) {
						FailTest(__LINE__);
					}
				}
			}
		}

		if (frameNumber_ == 25) {
			const FrameProfiler::Sample& sample = FrameProfiler::GetLatest();
			if (sample.number == 0 || sample.drawCalls == 0 || sample.uiDrawCalls == 0 || sample.frameMilliseconds <= 0) {
				FailTest(__LINE__);
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
					FailTest(__LINE__);
				} else {
					if (!window->Active) {
						FailTest(__LINE__);
					}
				}
			}
		}
		if (liveCount > 100) {
			FailTest(__LINE__);
		}
		if (frameNumber_ > 5 && liveCount > 0) {
			sawParticles_ = true;
		}
		if (frameNumber_ == 32) {
			pausedCount_ = liveCount;
		}
		if (frameNumber_ == 40 && liveCount != pausedCount_) {
			FailTest(__LINE__);
		}
		if (frameNumber_ == 46 && liveCount != 0) {
			FailTest(__LINE__);
		}
		if (frameNumber_ == 51 && liveCount == 0) {
			FailTest(__LINE__);
		}
		if (frameNumber_ == 66 && liveCount == 0) {
			FailTest(__LINE__);
		}
		if (frameNumber_ == 72) {
			RemoveObject(emitterId_);
		}
		if (frameNumber_ == 73 && liveCount != 0) {
			FailTest(__LINE__);
		}
		if (frameNumber_ == 60 && liveCount != 0) {
			FailTest(__LINE__);
		}
	}
	void CapturePreview() const {
		lightingTests_.Verify(*previewGraphics_);
		VerifySoftSamples();
		VerifyBlendSamples();
		DirectX::ScratchImage preview;
		if (FAILED(DirectX::CaptureTexture(previewGraphics_->GetCommandQueue(),previewGraphics_->GetPresentedBuffer(),false,preview,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_PRESENT))) {
			throw std::runtime_error("Editor preview capture failed.");
		}
		if (FAILED(DirectX::SaveToWICFile(*preview.GetImage(0,0,0),DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),L"editor-ui.png"))) {
			throw std::runtime_error("Editor preview PNG save failed.");
		}
	}
	bool Passed() const {
		// 途中終了と粒子未生成も識別し、一般的な失敗メッセージだけで原因を失わない。
		if (!valid_) {
			return false;
		}
		if (!sawParticles_) {
			std::cerr << "GPU test did not observe particles; frames=" << frameNumber_ << '\n';
			return false;
		}
		if (frameNumber_ < 80) {
			std::cerr << "GPU test stopped before completion; frames=" << frameNumber_ << '\n';
			return false;
		}
		return true;
	}
private:
	void FailTest(int sourceLine) {
		if (valid_) {
			std::cerr << "Editor test failure at line " << sourceLine << " frame " << frameNumber_ << std::endl;
		}
		valid_ = false;
	}
	// RGB画像でも黒背景が残らず、半透明のScene画像を作らないことを実GPUで確認する。
	struct BlendSample {
		Particle particle;
		SceneRenderTarget target;
	};
	void InitializeSoftSamples(const SceneContext& context) {
		softRenderer_ = context.particleRenderer;
		softObjects_ = context.object3dCommon;
		// Particle板と交差する斜めの実メッシュ。左は手前、右は奥になる。
		std::ofstream planeFile("resources/fixtures/soft-plane.obj");
		planeFile << "v -2 -2 -0.35\nv -2 2 -0.35\nv 2 -2 0.45\nv 2 2 0.45\n"
			<< "vt 0 1\nvt 0 0\nvt 1 1\nvt 1 0\nvn 0 0 -1\nf 1/1/1 2/2/1 3/3/1\nf 3/3/1 2/2/1 4/4/1\n";
		planeFile.close();
		std::string error;
		if (!PrepareModel("fixtures/soft-plane.obj",error)) {
			throw std::runtime_error(error);
		}
		softPlane_ = std::make_unique<Object3d>();
		softPlane_->Initialize(context.object3dCommon);
		softPlane_->SetModel(ModelManager::GetInstance()->FindModel("fixtures/soft-plane.obj"));
		softPlane_->SetUseRootNodeMatrix(false);
		softPlane_->SetEnableLighting(false);
		softPlane_->SetTexture("resources/white1x1.png");
		softPlane_->SetColor({0.055f,0.065f,0.085f,1});
		for (size_t sampleIndex = 0; sampleIndex < softSamples_.size(); ++sampleIndex) {
			BlendSample& sample = softSamples_[sampleIndex];
			sample.target.Initialize(previewGraphics_,context.object3dCommon->GetSrvManager());
			sample.target.Resize(128,128);
			sample.particle.Initialize(context.particleRenderer);
			ParticleSettings settings;
			settings.spread = {};
			settings.velocityMinimum = {};
			settings.velocityMaximum = {};
			settings.spawnCount = 1;
			settings.particleScale = {0.8f,0.8f,0.8f};
			settings.color = {0.8f,0.3f,0.03f,1};
			settings.blendMode = kBlendModeNormal;
			settings.softParticles = sampleIndex%2 == 1;
			if (sampleIndex >= 4) {
				settings.lifetimeAppearance = true;
				settings.startSize = 1;
				settings.endSize = 2;
				settings.color = {1,0,0,1};
				settings.endColor = {0,0,1,1};
				settings.lifetimeMinimum = 1;
				settings.lifetimeMaximum = 1;
				settings.softParticles = false;
			}
			if (!sample.particle.ApplySettings(settings)) {
				throw std::runtime_error("Soft particle settings rejected.");
			}
			sample.particle.AddParticles();
			if (sampleIndex == 5) {
				const Matrix4x4 camera = MathUtility::MakeTranslateMatrix({0,0,-3});
				const Matrix4x4 projection = MathUtility::Multiply(MathUtility::Inverse(camera),MathUtility::MakePerspectiveFovMatrix(0.8f,1,0.1f,10));
				for (size_t frameIndex = 0; frameIndex < 30; ++frameIndex) {
					sample.particle.Update(projection,camera,true,false);
				}
			}
		}
	}
	void DrawSoftSamples() {
		const Matrix4x4 camera = MathUtility::MakeTranslateMatrix({0,0,-3});
		const Matrix4x4 projection = MathUtility::Multiply(MathUtility::Inverse(camera),
			MathUtility::MakePerspectiveFovMatrix(0.8f,1,0.1f,10));
		softPlane_->Update(projection,MathUtility::MakeIdentity4x4());
		for (size_t sampleIndex = 0; sampleIndex < softSamples_.size(); ++sampleIndex) {
			BlendSample& sample = softSamples_[sampleIndex];
			if (sampleIndex == 1) {
				// 参照深度のリサイズ後も、SRVと復元座標の寸法を揃える。
				const uint32_t size = 128+(frameNumber_%2)*64;
				sample.target.Resize(size,size);
			}
			sample.target.Begin();
			if (sampleIndex < 2) {
				softObjects_->DrawSettingCommon();
				softPlane_->Draw();
			}
			softRenderer_->BeginPass();
			sample.particle.Update(projection,camera,false,false);
			sample.particle.Draw();
			softRenderer_->EndPass();
			sample.target.End();
		}
	}
	void VerifySoftSamples() const {
		std::array<DirectX::ScratchImage,6> captures;
		for (size_t sampleIndex = 0; sampleIndex < softSamples_.size(); ++sampleIndex) {
			if (FAILED(DirectX::CaptureTexture(previewGraphics_->GetCommandQueue(),softSamples_[sampleIndex].target.GetColorResource(),false,captures[sampleIndex],
				D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))) {
				throw std::runtime_error("Soft particle GPU capture failed.");
			}
			captures[sampleIndex].OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
		}
		const DirectX::Image& hard = *captures[0].GetImage(0,0,0);
		const DirectX::Image& soft = *captures[1].GetImage(0,0,0);
		const uint8_t* hardCenter = hard.pixels+64*hard.rowPitch+64*4;
		const uint8_t* softCenter = soft.pixels+64*soft.rowPitch+64*4;
		if (hardCenter[0] < softCenter[0]+20 || softCenter[0] < soft.pixels[0]+2) {
			throw std::runtime_error("Soft particle intersection did not fade against real geometry.");
		}
		const DirectX::Image& emptyHard = *captures[2].GetImage(0,0,0);
		const DirectX::Image& emptySoft = *captures[3].GetImage(0,0,0);
		for (size_t rowIndex = 0; rowIndex < 128; ++rowIndex) {
			for (size_t byteIndex = 0; byteIndex < 128*4; ++byteIndex) {
				if (std::abs(static_cast<int>(emptyHard.pixels[rowIndex*emptyHard.rowPitch+byteIndex])-emptySoft.pixels[rowIndex*emptySoft.rowPitch+byteIndex]) > 1) {
					throw std::runtime_error("Soft particles faded against clear depth/sky.");
				}
			}
		}
		DirectX::SaveToWICFile(hard,DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),L"particle-hard-intersection.png");
		DirectX::SaveToWICFile(soft,DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),L"particle-soft-intersection.png");
		const DirectX::Image& birth = *captures[4].GetImage(0,0,0);
		const DirectX::Image& middle = *captures[5].GetImage(0,0,0);
		const uint8_t* birthCenter = birth.pixels+64*birth.rowPitch+64*4;
		const uint8_t* middleCenter = middle.pixels+64*middle.rowPitch+64*4;
		if (birthCenter[0] < middleCenter[0]+20 || middleCenter[2] < birthCenter[2]+20) {
			throw std::runtime_error("Lifetime color interpolation did not reach GPU.");
		}
		size_t birthPixels = 0;
		size_t middlePixels = 0;
		for (size_t rowIndex = 0; rowIndex < 128; ++rowIndex) {
			for (size_t columnIndex = 0; columnIndex < 128; ++columnIndex) {
				const uint8_t* first = birth.pixels+rowIndex*birth.rowPitch+columnIndex*4;
				const uint8_t* second = middle.pixels+rowIndex*middle.rowPitch+columnIndex*4;
				if (first[0] > birth.pixels[0]+10) {
					++birthPixels;
				}
				if (second[2] > middle.pixels[2]+10) {
					++middlePixels;
				}
			}
		}
		if (middlePixels <= birthPixels) {
			throw std::runtime_error("Lifetime size interpolation did not reach GPU.");
		}
	}
	void InitializeBlendSamples(const SceneContext& context) {
		// 色は赤、透明度だけを変えた試験画像。RGBを誤ってマスク扱いしないことも検証する。
		DirectX::ScratchImage alphaFixture;
		if (FAILED(alphaFixture.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM,16,16,1,1))) {
			throw std::runtime_error("Particle alpha fixture allocation failed.");
		}
		const DirectX::Image& alphaImage = *alphaFixture.GetImage(0,0,0);
		for (size_t rowIndex = 0; rowIndex < 16; ++rowIndex) {
			for (size_t columnIndex = 0; columnIndex < 16; ++columnIndex) {
				uint8_t* pixel = alphaImage.pixels+rowIndex*alphaImage.rowPitch+columnIndex*4;
				pixel[0] = 255;
				pixel[1] = 0;
				pixel[2] = 0;
				pixel[3] = 0;
				if (rowIndex >= 4 && rowIndex < 12) {
					if (columnIndex >= 4 && columnIndex < 12) {
						pixel[3] = 255;
					}
				}
			}
		}
		if (FAILED(DirectX::SaveToWICFile(alphaImage,DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),L"resources/fixtures/particle-alpha.png"))) {
			throw std::runtime_error("Particle alpha fixture save failed.");
		}
		const float opacity[3] = {1,0.25f,0.0001f};
		for (size_t sampleIndex = 0; sampleIndex < blendSamples_.size(); ++sampleIndex) {
			BlendSample& sample = blendSamples_[sampleIndex];
			sample.target.Initialize(previewGraphics_,context.object3dCommon->GetSrvManager());
			sample.target.Resize(128,128);
			sample.particle.Initialize(context.particleRenderer);
			ParticleSettings settings;
			settings.spread = {};
			settings.velocityMinimum = {};
			settings.velocityMaximum = {};
			settings.spawnCount = 1;
			settings.particleScale = {0.8f,0.8f,0.8f};
			settings.color = {0.8f,0.3f,0.03f,opacity[sampleIndex%3]};
			settings.blendMode = static_cast<BlendMode>((sampleIndex/3)%8+1);
			if (settings.blendMode == kBlendModeDarken) {
				// 暗い背景より暗い色を用い、比較(暗)の描画確認を成立させる。
				settings.texture = "resources/white1x1.png";
			}
			if (!sample.particle.ApplySettings(settings)) {
				throw std::runtime_error("Particle blend fixture settings rejected.");
			}
			if (sampleIndex >= 24) {
				// Editorの画像候補とは独立に、公開描画APIでAlpha付き素材を確認する。
				sample.particle.SetTexture("resources/fixtures/particle-alpha.png");
			}
			sample.particle.AddParticles();
		}
		if (TextureManager::GetInstance()->HasTransparency("resources/circle.png")) {
			throw std::runtime_error("Legacy particle fixture unexpectedly has alpha.");
		}
		if (!TextureManager::GetInstance()->HasTransparency("resources/fixtures/particle-alpha.png")) {
			throw std::runtime_error("Authored texture alpha was lost.");
		}
	}
	void DrawBlendSamples() {
		const Matrix4x4 camera = MathUtility::MakeTranslateMatrix({0,0,-3});
		const Matrix4x4 projection = MathUtility::Multiply(MathUtility::Inverse(camera),
			MathUtility::MakePerspectiveFovMatrix(0.8f,1,0.1f,10));
		for (BlendSample& sample : blendSamples_) {
			sample.target.Begin();
			sample.particle.Update(projection,camera,false,false);
			sample.particle.Draw();
			sample.target.End();
		}
	}
	void VerifyBlendSamples() const {
		for (size_t sampleIndex = 0; sampleIndex < blendSamples_.size(); ++sampleIndex) {
			DirectX::ScratchImage captured;
			if (FAILED(DirectX::CaptureTexture(previewGraphics_->GetCommandQueue(),blendSamples_[sampleIndex].target.GetColorResource(),
				false,captured,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE))) {
				throw std::runtime_error("Particle blend capture failed.");
			}
			// GPUリソースはTYPELESS、RTV/SRVはsRGB。PNG書出しにも実際の解釈を渡す。
			captured.OverrideFormat(DXGI_FORMAT_R8G8B8A8_UNORM_SRGB);
			const DirectX::Image& frame = *captured.GetImage(0,0,0);
			const uint8_t* background = frame.pixels;
			int maximumDifference = 0;
			for (size_t rowIndex = 0; rowIndex < frame.height; ++rowIndex) {
				for (size_t columnIndex = 0; columnIndex < frame.width; ++columnIndex) {
					const uint8_t* pixel = frame.pixels+rowIndex*frame.rowPitch+columnIndex*4;
					if (pixel[3] != 255) {
						throw std::runtime_error("Particle made opaque Scene target transparent.");
					}
					for (size_t channelIndex = 0; channelIndex < 3; ++channelIndex) {
						maximumDifference = (std::max)(maximumDifference,std::abs(static_cast<int>(pixel[channelIndex])-background[channelIndex]));
					}
				}
			}
			if (sampleIndex%3 == 2) {
				if (maximumDifference > 2) {
					throw std::runtime_error("Particle blend ignored opacity fade: "+std::to_string(sampleIndex/3+1));
				}
			} else if (sampleIndex%3 == 0 && maximumDifference == 0) {
				throw std::runtime_error("Particle blend fixture did not draw.");
			}
			if (sampleIndex == 0) {
				// 通常合成で板の内側の角に黒背景が出ないことを確認する。
				const uint8_t* corner = frame.pixels+25*frame.rowPitch+25*4;
				for (size_t channelIndex = 0; channelIndex < 3; ++channelIndex) {
					if (std::abs(static_cast<int>(corner[channelIndex])-background[channelIndex]) > 2) {
						throw std::runtime_error("Opaque particle texture retained rectangular background.");
					}
				}
				if (FAILED(DirectX::SaveToWICFile(frame,DirectX::WIC_FLAGS_NONE,DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),L"particle-normal.png"))) {
					throw std::runtime_error("Particle blend preview save failed.");
				}
			}
			if (sampleIndex == 24) {
				const uint8_t* transparent = frame.pixels+30*frame.rowPitch+30*4;
				for (size_t channelIndex = 0; channelIndex < 3; ++channelIndex) {
					if (transparent[channelIndex] != background[channelIndex]) {
						throw std::runtime_error("Authored alpha was replaced with RGB brightness.");
					}
				}
			}
		}
	}
	// 本番の更新経路で落下・接地・Pause・停止時の復元を確認する。
	void VerifyPhysicsSimulation() {
		const std::vector<WorldObjectData> original = CaptureObjects();
		std::string error;
		if (!ReplaceObjects({},error)) {
			throw std::runtime_error(error);
		}
		EditorSession session;
		session.Initialize(*this);
		// Projectドロップと同じ配置処理で実Assetを読み、Componentを編集する。
		const WorldObjectId floorId = PlaceModel("plane.gltf",0,{0,-0.5f,0},error);
		const WorldObjectId ballId = PlaceModel("axis.obj",0,{0,3,0},error);
		if (floorId == 0 || ballId == 0) {
			throw std::runtime_error("Production asset placement failed: "+error);
		}
		GetEditableObject(0).collider->present = true;
		GetEditableObject(0).collider->size = {20,1,20};
		GetEditableObject(1).collider->present = true;
		GetEditableObject(1).collider->shape = ColliderShape::Sphere;
		GetEditableObject(1).body->present = true;
		session.Update(*this,false);
		if (!session.Undo(*this,error) || GetEditableObjectCount() != 0) {
			throw std::runtime_error("Placed asset/component undo failed.");
		}
		if (!session.Redo(*this,error) || GetEditableObjectCount() != 2) {
			throw std::runtime_error("Placed asset/component redo failed.");
		}
		const std::filesystem::path workflowPath = "resources/scenes/placement-workflow.ponscene";
		if (!SceneFile::Save(*this,workflowPath,error)) {
			throw std::runtime_error("Placement workflow save failed: "+error);
		}
		if (!session.BeginPlay(*this,error)) {
			throw std::runtime_error(error);
		}
		OnPlayStateChanged(true);
		for (size_t frame = 0; frame < 180; ++frame) {
			GameScene::UpdateEditorFrame(nullptr,true);
		}
		if (std::abs(GetObjectWorldPosition(ballId).y-0.5f) > 0.002f || !IsColliderTouching(ballId)) {
			throw std::runtime_error("GameScene gravity did not land body on floor.");
		}
		const float height = GetObjectWorldPosition(ballId).y;
		session.TogglePause();
		GameScene::UpdateEditorFrame(nullptr,session.ShouldSimulate());
		if (GetObjectWorldPosition(ballId).y != height) {
			throw std::runtime_error("Physics advanced during Pause.");
		}
		if (!session.Stop(*this,error)) {
			throw std::runtime_error(error);
		}
		OnPlayStateChanged(false);
		if (GetObjectWorldPosition(ballId).y != 3 || IsColliderTouching(ballId)) {
			throw std::runtime_error("Physics Stop did not restore position and clear contacts.");
		}
		session.Initialize(*this);
		if (!ReplaceObjects({},error) || !SceneFile::Load(*this,workflowPath,error)) {
			throw std::runtime_error("Placement workflow reload failed: "+error);
		}
		if (session.IsDirty(*this) || GetObjectData(0)->model != "plane.gltf" || GetObjectData(1)->model != "axis.obj") {
			throw std::runtime_error("Reload changed placed assets or components.");
		}
		// 読み直した設定でも再び落下することを確認し、保存された見た目だけの検証にしない。
		OnPlayStateChanged(true);
		GameScene::UpdateEditorFrame(nullptr,true);
		if (GetObjectWorldPosition(ballId).y >= 3) {
			throw std::runtime_error("Reloaded Rigidbody did not simulate.");
		}
		OnPlayStateChanged(false);
		if (!ReplaceObjects(original,error)) {
			throw std::runtime_error(error);
		}
	}
	// 本番のGameScene経路で接触・Pause・Console通知・リセットを確認する。
	void VerifyColliderSimulation() {
		const std::vector<WorldObjectData> original = CaptureObjects();
		WorldObjectData first;
		first.id = 900001;
		first.collider.present = true;
		WorldObjectData second = first;
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
		const std::vector<WorldObjectData> original = CaptureObjects();
		WorldObjectData moving;
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
		const std::vector<WorldObjectData> original = CaptureObjects();
		std::string error;
		WorldObjectData triangle;
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
			FailTest(__LINE__);
		}
		if (std::abs(distance - 5) > 0.001f) {
			FailTest(__LINE__);
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
			FailTest(__LINE__);
		}
		camera.SetEditorInputEnabled(true);
		camera.Update();
		if (std::abs(camera.GetTransform().translate.z + 28) > 0.001f) {
			FailTest(__LINE__);
		}
		inputState.MouseWheel = 0;
		inputState.MouseDown[ImGuiMouseButton_Middle] = true;
		inputState.MouseDelta = {10, 5};
		camera.Update();
		if (std::abs(camera.GetTransform().translate.x + 0.2f) > 0.001f || std::abs(camera.GetTransform().translate.y - 0.1f) > 0.001f) {
			FailTest(__LINE__);
		}
		inputState.MouseDown[ImGuiMouseButton_Middle] = false;
		inputState.MouseDown[ImGuiMouseButton_Right] = true;
		camera.Update();
		if (std::abs(camera.GetTransform().rotate.x - 0.025f) > 0.001f || std::abs(camera.GetTransform().rotate.y - 0.05f) > 0.001f) {
			FailTest(__LINE__);
		}
		const TransformData beforeTextInput = camera.GetTransform();
		inputState.WantTextInput = true;
		inputState.MouseWheel = 2;
		camera.Update();
		if (camera.GetTransform().translate.z != beforeTextInput.translate.z || camera.GetTransform().rotate.y != beforeTextInput.rotate.y) {
			FailTest(__LINE__);
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
		std::vector<WorldObjectData> snapshot = CaptureObjects();
		WorldObjectData disabled;
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
				FailTest(__LINE__);
			} else {
				if (window->DockId == 0) {
					FailTest(__LINE__);
				}
			}
		}
		const char* optionalWindows[] = {"Toolbar", "Particle Editor", "World Settings", "Camera", "Performance"};
		for (const char* windowName : optionalWindows) {
			const ImGuiWindow* window = ImGui::FindWindowByName(windowName);
			if (window != nullptr) {
				if (window->Active) {
					FailTest(__LINE__);
				}
			}
		}
		ImGuiWindow* viewportWindow = ImGui::FindWindowByName("Scene Viewport");
		if (viewportWindow != nullptr) {
			ImGuiTabBar* tabBar = ImGui::GetCurrentContext()->TabBars.GetByKey(viewportWindow->GetID("SceneViewTabs"));
			if (tabBar == nullptr) {
				FailTest(__LINE__);
			} else {
				if (tabBar->Tabs.Size != 2) {
					FailTest(__LINE__);
				} else {
					if (std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[0])) != "SceneView") {
						FailTest(__LINE__);
					}
					if (std::string(ImGui::TabBarGetTabName(tabBar, &tabBar->Tabs[1])) != "GameView") {
						FailTest(__LINE__);
					}
				}
			}
		}
		if (ImGui::GetStyle().Colors[ImGuiCol_TabSelectedOverline].w != 0) {
			FailTest(__LINE__);
		}
		if (ImGui::GetStyle().Colors[ImGuiCol_TabSelected].x != ImGui::GetStyle().Colors[ImGuiCol_Tab].x) {
			FailTest(__LINE__);
		}
		if (std::abs(ImGui::GetStyle().FontScaleMain - 0.85f) > 0.001f) {
			FailTest(__LINE__);
		}
		if (!ImGui::GetFont()->IsGlyphInFont(0x7C92)) {
			FailTest(__LINE__);
		}
	}

	// テスト用メッセージはこのスレッドだけに送り、他アプリの入力は操作しない。
	void VerifyMessageQueue() {
		constexpr UINT kTestMessage = WM_APP + 37;
		MSG message{};
		PeekMessage(&message, nullptr, 0, 0, PM_NOREMOVE);
		for (uint32_t messageIndex = 0; messageIndex < 128; ++messageIndex) {
			if (!PostThreadMessage(GetCurrentThreadId(), kTestMessage, messageIndex, 0)) {
				FailTest(__LINE__);
			}
		}
		WinApp messagePump;
		if (messagePump.ProcessMessage()) {
			FailTest(__LINE__);
		}
		if (PeekMessage(&message, nullptr, kTestMessage, kTestMessage, PM_NOREMOVE)) {
			FailTest(__LINE__);
		}
	}

	DirectXCommon* previewGraphics_ = nullptr; // テスト用。Engineの寿命中だけ借用する。
	LightingGraphicsTests lightingTests_;
	std::array<BlendSample,27> blendSamples_; // 8合成方式＋Alpha付き画像を3段階の透明度で検証する。
	std::array<BlendSample,6> softSamples_; // 実メッシュ／背景とSoft有無、寿命前半／中間を比較。
	std::unique_ptr<Object3d> softPlane_;
	ParticleRenderer* softRenderer_ = nullptr; // テストシーンよりEngine側が長く生存する。
	Object3dCommon* softObjects_ = nullptr;
	WorldObjectId hierarchyParentId_ = kInvalidWorldObjectId;
	WorldObjectId lightChildId_ = kInvalidWorldObjectId;
	WorldObjectId modelChildId_ = kInvalidWorldObjectId;
	WorldObjectId emitterId_ = kInvalidWorldObjectId;
	WorldObjectId animatedFirstId_ = 0;
	WorldObjectId animatedSecondId_ = 0;
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
		RunSceneViewInteractionTests();
		Engine engine;
		GraphicsTestScene* scene = new GraphicsTestScene();
		engine.Initialize(scene);
		engine.Run(90);
		scene->CapturePreview();
		bool passed = scene->Passed();
		if (engine.HasGraphicsErrors()) {
			std::cerr << "GPU validation reported an error.\n";
			passed = false;
		}
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
