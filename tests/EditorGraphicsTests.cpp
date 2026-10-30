#include "Engine.h"
#include "GameScene.h"
#include "LogSystem.h"
#include "SceneRenderTarget.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <iostream>
#include <cmath>
#include <stdexcept>

// 実GPUでEditorを描画しながら、高頻度の発生・Pause・Burst・Resetを試験する。
class GraphicsTestScene : public GameScene {
public:
	void Initialize(const SceneContext& context) override {
		GameScene::Initialize(context);
		VerifyMessageQueue();
		resizeTarget_.Initialize(context.object3dCommon->GetDxCommon(), context.object3dCommon->GetSrvManager());
		originalHandle_ = resizeTarget_.GetTextureHandle();
		hierarchyParentId_ = CreateObject(SceneObjectType::Empty);
		lightChildId_ = CreateObject(SceneObjectType::PointLight);
		emitterId_ = CreateObject(SceneObjectType::Particle);
		CreateObject(SceneObjectType::Particle); // 複数EmitterのGPUリソースが共存できることも確認する。
		modelChildId_ = AddObject("axis.obj");
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
		}
		if (frameNumber_ == 18) {
			visibility = {};
		}
		GameScene::DrawEditorTools(visibility);
	}
	void UpdateEditorFrame(Input* input, bool simulate) override {
		(void)simulate;
		++frameNumber_;
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
			const char* tools[] = {"Particle Editor", "World Settings", "Camera"};
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
	bool Passed() const { return valid_ && sawParticles_ && frameNumber_ >= 80; }
private:
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
		const char* optionalWindows[] = {"Toolbar", "Particle Editor", "World Settings", "Camera"};
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

	SceneObjectId hierarchyParentId_ = kInvalidSceneObjectId;
	SceneObjectId lightChildId_ = kInvalidSceneObjectId;
	SceneObjectId modelChildId_ = kInvalidSceneObjectId;
	SceneObjectId emitterId_ = kInvalidSceneObjectId;
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
