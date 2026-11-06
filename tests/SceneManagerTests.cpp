#include "SceneManager.h"
#include <stdexcept>
#include <string>

namespace {
	struct SceneCounts {
		int initialized;
		int updated;
		int drawn;
		int finalized;
		int destroyed;
		bool isPlaying;
	};

	class TrackedScene : public IScene {
	public:
		explicit TrackedScene(SceneCounts& counts) : counts_(counts) {}
		~TrackedScene() override { ++counts_.destroyed; }
		void Initialize(const SceneContext&) override { ++counts_.initialized; }
		void Update(Input*) override { ++counts_.updated; }
		void Draw() override { ++counts_.drawn; }
		void Finalize() override { ++counts_.finalized; }
		void OnPlayStateChanged(bool isPlaying) override { counts_.isPlaying = isPlaying; }
		const char* GetName() const override { return "TrackedScene"; }
		void Queue(std::unique_ptr<IScene> nextScene) { RequestSceneChange(std::move(nextScene)); }
	private:
		SceneCounts& counts_;
	};

	void RequireSceneState(bool isConditionSatisfied, const char* message) {
		if (!isConditionSatisfied) {
			throw std::runtime_error(message);
		}
	}
}

// GPUを使わず、更新・描画中に破棄しないことと、再生停止時の所有権の復帰を確認する。
void RunSceneManagerTests() {
	SceneCounts original{};
	SceneCounts game{};
	SceneCounts title{};
	SceneCounts cancelled{};
	SceneManager manager;
	TrackedScene* originalScene = new TrackedScene(original);
	manager.Initialize({}, originalScene);
	RequireSceneState(std::string(manager.GetSceneName()) == "TrackedScene", "Scene name was not exposed.");
	manager.Update(nullptr, false);
	// 一時停止中はシーンを保持したまま、ゲームの更新回数が増えないことを確認する。
	RequireSceneState(original.updated == 0, "Paused scene advanced.");
	manager.SetPlayState(true);
	TrackedScene* gameScene = new TrackedScene(game);
	originalScene->Queue(std::unique_ptr<IScene>(gameScene));
	manager.Update(nullptr);
	manager.Draw();
	RequireSceneState(original.drawn == 1 && game.initialized == 0, "Scene switched before frame completion.");
	RequireSceneState(manager.ApplyPendingScene(), "Pending scene was not applied.");
	RequireSceneState(original.finalized == 0 && original.destroyed == 0, "Editor origin was destroyed during play.");
	RequireSceneState(game.initialized == 1 && game.isPlaying, "Next scene did not inherit play state.");
	TrackedScene* titleScene = new TrackedScene(title);
	gameScene->Queue(std::unique_ptr<IScene>(titleScene));
	manager.Update(nullptr);
	manager.Draw();
	RequireSceneState(game.finalized == 0, "Runtime scene destroyed before draw completion.");
	RequireSceneState(manager.ApplyPendingScene(), "Second scene was not applied.");
	RequireSceneState(game.finalized == 1 && game.destroyed == 1, "Runtime scene lifetime was not balanced.");
	titleScene->Queue(std::make_unique<TrackedScene>(cancelled));
	manager.Update(nullptr);
	manager.SetPlayState(false);
	RequireSceneState(title.finalized == 0, "Stop destroyed the scene before frame completion.");
	RequireSceneState(manager.ApplyPendingScene(), "Stop did not restore the original scene.");
	RequireSceneState(original.initialized == 1 && !original.isPlaying, "Restored scene was reinitialized or kept playing.");
	RequireSceneState(title.finalized == 1 && title.destroyed == 1, "Stopped scene resources were not released.");
	RequireSceneState(cancelled.initialized == 0 && cancelled.finalized == 0 && cancelled.destroyed == 1,
		"An uninitialized request was finalized or leaked.");
	RequireSceneState(!manager.ApplyPendingScene(), "A scene switch repeated without a request.");
	manager.Draw();
	RequireSceneState(original.drawn == 2, "Stop did not restore the original instance.");
	// 編集用の退避を使わない通常遷移は、直前の場面をそのまま解放する。
	originalScene->Queue(std::make_unique<TrackedScene>(game));
	manager.Update(nullptr);
	RequireSceneState(manager.ApplyPendingScene(), "Normal scene transition failed.");
	RequireSceneState(original.finalized == 1 && original.destroyed == 1, "Normal transition retained its old scene.");
	manager.Finalize();
	manager.Finalize();
	RequireSceneState(game.initialized == 2 && game.finalized == 2 && game.destroyed == 2, "Finalize was not balanced.");
}
