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
		uint64_t originPreviewHandle = 0;
		uint64_t returnPreviewHandle = 0;
		uint64_t restoredPreviewHandle = 0;

		bool wasRestored = false;
		bool wasUndone = false;
		bool wasRedone = false;
	};

	class TransitionTitleScene : public TitleScene {
	public:
		TransitionTitleScene(TransitionState& state, bool isOriginScene) : state_(state), isOriginScene_(isOriginScene) {}
		void Initialize(const SceneContext& context) override {
			TitleScene::Initialize(context);
			if (isOriginScene_) {
				++state_.originInitializations;
				GetEditableObject(0).transform->translate.x = 2;
			}
		}
		void Update(Input* input) override;
		void UpdateEditorFrame(Input* input, bool shouldSimulate) override {
			TitleScene::UpdateEditorFrame(input, shouldSimulate);
			if (isOriginScene_) {
				if (state_.frame >= 21) {
					if (GetEditableObject(0).transform->translate.x == 2) {
						state_.wasUndone = true;
					}
				}
				if (state_.wasUndone) {
					if (state_.frame >= 27) {
						if (GetEditableObject(0).transform->translate.x == 3) {
							state_.wasRedone = true;
						}
					}
				}
			}
			if (isOriginScene_) {
				// 再生後の復帰段階で、ゲームを進めずに編集時の座標へ戻ったことを確認する。
				if (state_.phase == 2) {
					if (!shouldSimulate) {
						if (GetEditableObject(0).transform->translate.x == 3) {
							state_.wasRestored = true;

							// 停止で元のシーンへ戻ったとき、再生前の画像をもう一度取得する。
							if (state_.restoredPreviewHandle == 0) {
								std::string error;

								state_.restoredPreviewHandle = GetModelPreview("teapot.obj", error);
							}
						}
					}
				}
			}
		}
		void DrawEditorTools(EditorToolVisibility& visibility) override {
			TitleScene::DrawEditorTools(visibility);
			++state_.frame;

			// 同じモデルを、再生前のシーンと遷移先のシーンからそれぞれ要求する。
			if (isOriginScene_) {
				if (state_.originPreviewHandle == 0) {
					std::string error;

					state_.originPreviewHandle = GetModelPreview("teapot.obj", error);
				}
			} else if (state_.returnPreviewHandle == 0) {
				std::string error;

				state_.returnPreviewHandle = GetModelPreview("teapot.obj", error);
			}

			if (isOriginScene_) {
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
			if (isOriginScene_) {
				++state_.originFinalizations;
			} else {
				++state_.returnFinalizations;
			}
			TitleScene::Finalize();
		}
	private:
		TransitionState& state_;
		bool isOriginScene_ = false;
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
		if (isOriginScene_) {
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
	const bool hasGraphicsErrors = engine.HasGraphicsErrors();
	engine.Finalize();
	if (hasGraphicsErrors) {
		throw std::runtime_error("GPU validation failed during scene transitions.");
	}

	// 遷移先は専用の画像を持ち、停止後には元のシーンのハンドルが戻る。
	if (state.originPreviewHandle == 0 || state.returnPreviewHandle == 0) {
		throw std::runtime_error("The title scenes did not prepare model previews.");
	}

	if (state.originPreviewHandle == state.returnPreviewHandle) {
		throw std::runtime_error("The destination scene reused the origin scene's preview.");
	}

	if (state.restoredPreviewHandle != state.originPreviewHandle) {
		throw std::runtime_error("Stopping play did not restore the origin scene's preview.");
	}

	if (state.phase != 2 || !state.wasRestored || !state.wasUndone || !state.wasRedone || state.originInitializations != 1 ||
		state.originFinalizations != 1 || state.gameFinalizations != 1 || state.returnFinalizations != 1) {
		std::cerr << "phase=" << state.phase << " restored=" << state.wasRestored
			<< " undo/redo=" << state.wasUndone << "/" << state.wasRedone
			<< " origin init/final=" << state.originInitializations << "/" << state.originFinalizations
			<< " game final=" << state.gameFinalizations << " return final=" << state.returnFinalizations << "\n";
		throw std::runtime_error("Title/Game transitions or editor Stop restoration failed.");
	}
	std::cout << "GPU scene transitions and editor Stop restoration passed\n";
}
