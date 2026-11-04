#include "SceneTransitionGraphicsTests.h"
#include "Engine.h"
#include "GameScene.h"
#include "TitleScene.h"
#include "imgui/imgui.h"
#include <stdexcept>
#include <iostream>

namespace {
	struct TransitionState {
		int frame = 0;
		int phase = 0;
		int originInitializations = 0;
		int originFinalizations = 0;
		int gameFinalizations = 0;
		int returnFinalizations = 0;
		bool restored = false;
		bool undone = false;
		bool redone = false;
	};

	class TransitionTitleScene : public TitleScene {
	public:
		TransitionTitleScene(TransitionState& state, bool origin) : state_(state), origin_(origin) {}
		void Initialize(const SceneContext& context) override {
			TitleScene::Initialize(context);
			if (origin_) {
				++state_.originInitializations;
				GetEditableObject(0).transform->translate.x = 2;
			}
		}
		void Update(Input* input) override;
		void UpdateEditorFrame(Input* input, bool simulate) override {
			TitleScene::UpdateEditorFrame(input, simulate);
			if (origin_) {
				if (state_.frame >= 21) {
					if (GetEditableObject(0).transform->translate.x == 2) {
						state_.undone = true;
					}
				}
				if (state_.undone) {
					if (state_.frame >= 27) {
						if (GetEditableObject(0).transform->translate.x == 3) {
							state_.redone = true;
						}
					}
				}
			}
			if (origin_) {
				if (state_.phase == 2) {
					if (!simulate) {
						if (GetEditableObject(0).transform->translate.x == 3) {
							state_.restored = true;
						}
					}
				}
			}
		}
		void DrawEditorTools(EditorToolVisibility& visibility) override {
			TitleScene::DrawEditorTools(visibility);
			++state_.frame;
			if (origin_) {
				if (state_.frame == 1) {
					// 保存基準x=2から編集x=3を履歴へ入れ、遷移後もUndo/Redoできるか調べる。
					GetEditableObject(0).transform->translate.x = 3;
				}
			}
			if (state_.frame == 20) {
				ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
				ImGui::GetIO().AddKeyEvent(ImGuiKey_Z, true);
			}
			if (state_.frame == 21) {
				ImGui::GetIO().AddKeyEvent(ImGuiKey_Z, false);
				ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
			}
			if (state_.frame == 26) {
				ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
				ImGui::GetIO().AddKeyEvent(ImGuiKey_Y, true);
			}
			if (state_.frame == 27) {
				ImGui::GetIO().AddKeyEvent(ImGuiKey_Y, false);
				ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
			}
			// OSの入力を操作せず、自分のImGuiキューだけへCtrl+Pを送る。
			if (state_.frame == 2 || state_.frame == 12) {
				ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);
				ImGui::GetIO().AddKeyEvent(ImGuiKey_P, true);
			}
			if (state_.frame == 3 || state_.frame == 13) {
				ImGui::GetIO().AddKeyEvent(ImGuiKey_P, false);
				ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
			}
		}
		void Finalize() override {
			if (origin_) {
				++state_.originFinalizations;
			} else {
				++state_.returnFinalizations;
			}
			TitleScene::Finalize();
		}
	private:
		TransitionState& state_;
		bool origin_ = false;
	};

	class TransitionGameScene : public GameScene {
	public:
		explicit TransitionGameScene(TransitionState& state) : state_(state) {}
		void Update(Input* input) override {
			GameScene::Update(input);
			if (state_.phase == 1) {
				state_.phase = 2;
				RequestSceneChange(std::make_unique<TransitionTitleScene>(state_, false));
			}
		}
		void DrawEditorTools(EditorToolVisibility& visibility) override {
			GameScene::DrawEditorTools(visibility);
			++state_.frame;
			if (state_.frame == 3) {
				ImGui::GetIO().AddKeyEvent(ImGuiKey_P, false);
				ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);
			}
		}
		void Finalize() override {
			++state_.gameFinalizations;
			GameScene::Finalize();
		}
	private:
		TransitionState& state_;
	};

	void TransitionTitleScene::Update(Input* input) {
		TitleScene::Update(input);
		if (origin_) {
			if (state_.phase == 0) {
				// 再生前にx=3だった編集データが、遷移・停止後に戻ることを確認する。
				GetEditableObject(0).transform->translate.x = 9;
				state_.phase = 1;
				RequestSceneChange(std::make_unique<TransitionGameScene>(state_));
			}
		}
	}
}

void RunSceneTransitionGraphicsTests() {
	TransitionState state;
	Engine engine;
	engine.Initialize(new TransitionTitleScene(state, true));
	engine.Run(90);
	const bool graphicsErrors = engine.HasGraphicsErrors();
	engine.Finalize();
	if (graphicsErrors) {
		throw std::runtime_error("GPU validation failed during scene transitions.");
	}
	if (state.phase != 2 || !state.restored || !state.undone || !state.redone || state.originInitializations != 1 ||
		state.originFinalizations != 1 || state.gameFinalizations != 1 || state.returnFinalizations != 1) {
		std::cerr << "phase=" << state.phase << " restored=" << state.restored
			<< " undo/redo=" << state.undone << "/" << state.redone
			<< " origin init/final=" << state.originInitializations << "/" << state.originFinalizations
			<< " game final=" << state.gameFinalizations << " return final=" << state.returnFinalizations << "\n";
		throw std::runtime_error("Title/Game transitions or editor Stop restoration failed.");
	}
	std::cout << "GPU scene transitions and editor Stop restoration passed\n";
}
