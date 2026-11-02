#include "../project/CG2/editor/panels/scene_view/SceneViewPanel.h"
#include "../project/CG2/editor/EditorSelection.h"
#include "objects/WorldObjects.h"
#include "SceneViewMath.h"
#include "MathUtility.h"
#include "../project/CG2/editor/panels/inspector/InspectorPanel.h"
#include "../project/CG2/editor/panels/performance/PerformancePanel.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <cmath>
#include <stdexcept>
#include <string>

namespace {
	void RequireInteraction(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	// OSの入力やユーザーの画面へ触れず、専用ImGuiコンテキスト内だけで操作する。
	class InteractionContext {
	public:
		InteractionContext() : previous_(ImGui::GetCurrentContext()) {
			context_ = ImGui::CreateContext();
			ImGuiIO& input = ImGui::GetIO();
			input.IniFilename = nullptr;
			input.DisplaySize = {800,600};
			input.DeltaTime = 1.0f/60;
			// 一手の入力イベントを同じ試験フレームで処理し、OSの到着時刻には依存させない。
			input.ConfigInputTrickleEventQueue = false;
			input.Fonts->AddFontDefault();
			input.Fonts->Build();
		}
		~InteractionContext() {
			ImGui::DestroyContext(context_);
			ImGui::SetCurrentContext(previous_);
		}
	private:
		ImGuiContext* previous_ = nullptr;
		ImGuiContext* context_ = nullptr;
	};

	class InteractionScene : public WorldObjects {
	public:
		void SetEditorGameView(bool enabled) override { gameView = enabled; }
		void SetEditorCameraInput(bool enabled) override { cameraInput = enabled; }
		void SetEditorViewport(uint32_t width, uint32_t height) override {
			view.viewportWidth = static_cast<float>(width);
			view.viewportHeight = static_cast<float>(height);
			view.cameraPosition = {0,0,-10};
			view.viewProjection = MathUtility::Multiply(MathUtility::MakeTranslateMatrix({0,0,10}),
				MathUtility::MakePerspectiveFovMatrix(0.8f,view.viewportWidth/view.viewportHeight,0.1f,100));
		}
		bool GetEditorViewInfo(EditorViewInfo& result) const override {
			imageOrigin = ImGui::GetItemRectMin();
			result = view;
			return true;
		}
		EditorViewInfo view{};
		mutable ImVec2 imageOrigin{};
		bool gameView = false;
		bool cameraInput = false;
	};

	class InteractionFixture {
	public:
		InteractionFixture() {
			selection.Select(scene.CreateObject(WorldObjectType::Empty));
			panel.Initialize();
			panel.SetTexture(1); // GPUへ送信しない専用コンテキストの仮ハンドル。
			Draw();
			Draw();
		}
		void Draw() {
			ImGui::NewFrame();
			ImGui::SetNextWindowPos({0,0},ImGuiCond_Always);
			ImGui::SetNextWindowSize({800,600},ImGuiCond_Always);
			panel.Draw(scene,selection);
			ImGui::EndFrame();
		}
		ImVec2 Center() const {
			Vector2 projected{};
			RequireInteraction(SceneViewMath::Project(scene.GetObjectWorldPosition(selection.Get()),scene.view.viewProjection,
				scene.view.viewportWidth,scene.view.viewportHeight,projected),"Interaction pivot not visible");
			return {scene.imageOrigin.x+projected.x,scene.imageOrigin.y+projected.y};
		}
		void Mouse(const ImVec2& position, bool pressed) {
			ImGui::GetIO().AddMousePosEvent(position.x,position.y);
			Draw(); // Popupへ移った場合もホバー先を確定してから押下する。
			ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left,pressed);
			Draw();
		}
		void ChooseTool(size_t toolIndex) {
			const ImVec2 combo = {scene.imageOrigin.x+45,scene.imageOrigin.y-ImGui::GetFrameHeight()*0.5f-ImGui::GetStyle().ItemSpacing.y};
			Mouse(combo,true);
			Mouse(combo,false);
			Draw();
			Draw();
			const ImGuiWindow* popup = ImGui::FindWindowByName("##Combo_00");
			RequireInteraction(popup != nullptr && popup->Active,"Tool combo did not open");
			const ImVec2 item = {popup->Pos.x+30,popup->Pos.y+popup->WindowPadding.y+
				static_cast<float>(toolIndex)*ImGui::GetTextLineHeightWithSpacing()+ImGui::GetTextLineHeight()*0.5f};
			Mouse(item,true);
			Mouse(item,false);
			Draw();
		}
		// 公開タブを実際にクリックし、分離したGameViewへの切替を確認する。
		void ChooseView(size_t tabIndex) {
			Draw(); // 直前にInspectorだけを描いた場合も、対象ウィンドウを有効に戻す。
			ImGui::SetWindowFocus("Scene Viewport");
			ImGuiWindow* window = ImGui::FindWindowByName("Scene Viewport");
			RequireInteraction(window != nullptr,"Viewport window missing");
			ImGuiTabBar* tabs = ImGui::GetCurrentContext()->TabBars.GetByKey(window->GetID("SceneViewTabs"));
			RequireInteraction(tabs != nullptr,"Viewport tabs missing");
			RequireInteraction(tabIndex < static_cast<size_t>(tabs->Tabs.Size),"Viewport tab index missing");
			const ImGuiTabItem& tab = tabs->Tabs[static_cast<int>(tabIndex)];
			const ImVec2 position = {tabs->BarRect.Min.x+tab.Offset+tab.Width*0.5f,
				tabs->BarRect.Min.y+tabs->BarRect.GetHeight()*0.5f};
			Mouse(position,true);
			Mouse(position,false);
			Draw();
		}
		InteractionScene scene;
		EditorSelection selection;
		SceneViewPanel panel;
	};
}

// 公開Drawへマウス入力を流し、軸の選択・ドラッグ・スナップ・解除まで確認する。
void RunSceneViewInteractionTests() {
	InteractionContext context;
	InteractionFixture fixture;
	ImVec2 center = fixture.Center();
	fixture.Mouse({center.x+40,center.y},true);
	RequireInteraction(fixture.panel.IsManipulating(),"Move handle was not picked");
	fixture.Mouse({center.x+80,center.y},true);
	RequireInteraction(fixture.scene.GetObjectWorldPosition(fixture.selection.Get()).x > 0,"Move drag did not move object");
	fixture.Mouse({center.x+80,center.y},false);
	RequireInteraction(!fixture.panel.IsManipulating(),"Move drag did not finish");
	fixture.scene.GetEditableObject(0).transform->translate = {};
	fixture.Draw();
	center = fixture.Center();
	ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,true);
	fixture.Mouse({center.x+40,center.y},true);
	fixture.Mouse({center.x+95,center.y},true);
	const float moved = fixture.scene.GetObjectWorldPosition(fixture.selection.Get()).x;
	if (moved <= 0 || std::abs(moved-std::round(moved)) >= 0.001f) {
		throw std::runtime_error("Move Ctrl snap failed: "+std::to_string(moved)+" Ctrl="+std::to_string(ImGui::GetIO().KeyCtrl)+" drag="+std::to_string(fixture.panel.IsManipulating()));
	}
	fixture.Mouse({center.x+95,center.y},false);
	ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,false);
	fixture.scene.GetEditableObject(0).transform->translate = {};
	fixture.Draw();
	fixture.ChooseTool(1);
	center = fixture.Center();
	fixture.Mouse({center.x+56.5685f,center.y-56.5685f},true);
	RequireInteraction(fixture.panel.IsManipulating(),"Rotation ring was not picked");
	fixture.Mouse({center.x-56.5685f,center.y-56.5685f},true);
	RequireInteraction(std::abs(fixture.scene.GetEditableObject(0).transform->rotate.z-1.5707963f) < 0.02f,"Rotation did not follow mouse direction");
	fixture.Mouse({center.x-56.5685f,center.y-56.5685f},false);
	fixture.scene.GetEditableObject(0).transform->rotate = {};
	fixture.Draw();
	fixture.ChooseTool(2);
	center = fixture.Center();
	fixture.Mouse({center.x+40,center.y},true);
	ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,true);
	fixture.Mouse({center.x+62,center.y},true);
	RequireInteraction(std::abs(fixture.scene.GetEditableObject(0).transform->scale.x-1.3f) < 0.002f,"Scale ratio snap failed");
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,true);
	fixture.Draw();
	RequireInteraction(!fixture.panel.IsManipulating(),"Escape did not cancel scale drag");
	RequireInteraction(std::abs(fixture.scene.GetEditableObject(0).transform->scale.x-1) < 0.001f,"Escape did not restore initial scale");
	fixture.Mouse(center,false);
	ImGui::GetIO().AddKeyEvent(ImGuiKey_Escape,false);
	ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl,false);
	fixture.Draw();
	fixture.selection.Select(fixture.scene.CreateObject(WorldObjectType::Camera));
	fixture.selection.Toggle(fixture.scene.CreateObject(WorldObjectType::PointLight));
	fixture.Draw();
	ImGui::NewFrame();
	InspectorPanel::DrawSelection(fixture.scene,fixture.selection);
	ImGui::EndFrame();
	RequireInteraction(fixture.selection.GetIds().size() == 2,"Multiple Inspector changed the selection");
	for (WorldObjectType type : {WorldObjectType::PointLight,WorldObjectType::SpotLight,WorldObjectType::DirectionalLight}) {
		const WorldObjectId identifier = fixture.scene.CreateObject(type);
		fixture.selection.Select(identifier);
		fixture.Draw(); // 円錐・方向矢印を含む補助表示も同じ操作コンテキストで描く。
		ImGui::NewFrame();
		InspectorPanel::DrawSelection(fixture.scene,fixture.selection);
		ImGui::EndFrame();
	}
	const WorldObjectId selectedBeforeGameView = fixture.selection.Get();
	fixture.ChooseView(1);
	RequireInteraction(fixture.scene.gameView,"GameView tab did not select the game camera");
	RequireInteraction(!fixture.scene.cameraInput,"GameView enabled the editor camera input");
	RequireInteraction(fixture.selection.Get() == selectedBeforeGameView,"GameView changed object selection");
	RequireInteraction(!fixture.panel.IsManipulating(),"GameView retained a gizmo drag");
	fixture.panel.ShowScene();
	fixture.Draw();
	fixture.Draw();
	RequireInteraction(!fixture.scene.gameView,"SceneView could not be restored after GameView");
	// OptionalなPerformanceも専用パネルとしてBegin/Endを完結させる。
	PerformancePanel performance;
	bool performanceVisible = true;
	ImGui::NewFrame();
	performance.Draw(performanceVisible);
	ImGui::EndFrame();
	const ImGuiWindow* performanceWindow = ImGui::FindWindowByName("Performance");
	RequireInteraction(performanceWindow != nullptr,"Performance panel did not create its window");
}

