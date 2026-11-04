#include "SceneManager.h"
#include <cassert>
#include <utility>

//=============================================================================================//
// 初期化処理

void SceneManager::Initialize(const SceneContext& context, IScene* firstScene) {
	assert(firstScene != nullptr);

	assert(currentScene_ == nullptr);

	context_ = context;

	currentScene_.reset(firstScene);

	currentScene_->Initialize(context_);
}

//=============================================================================================//
// 更新処理

void SceneManager::Update(Input* input, bool simulate) {
	if (currentScene_ == nullptr) {
		return;
	}

	currentScene_->UpdateEditorFrame(input, simulate);

	if (pendingScene_ == nullptr) {
		pendingScene_ = currentScene_->TakeNextScene();
	}
}

bool SceneManager::ApplyPendingScene() {
	if (restoreEditScene_) {
		// 停止と同じフレームに遷移要求があっても、元の場面への復帰を優先する。
		pendingScene_.reset();

		ReleaseScene(currentScene_);

		currentScene_ = std::move(editScene_);

		restoreEditScene_ = false;

		currentScene_->OnPlayStateChanged(false);

		return true;
	}

	if (pendingScene_ == nullptr) {
		return false;
	}

	if (playing_) {
		if (editScene_ == nullptr) {
			// EditorSessionの停止前スナップショットが参照する場面を保持する。
			currentScene_->OnPlayStateChanged(false);

			editScene_ = std::move(currentScene_);
		}
	}

	ReleaseScene(currentScene_);

	currentScene_ = std::move(pendingScene_);

	currentScene_->Initialize(context_);

	if (playing_) {
		currentScene_->OnPlayStateChanged(true);
	}

	return true;
}

//=============================================================================================//
// 描画処理

void SceneManager::Draw() {
	if (currentScene_ != nullptr) {
		currentScene_->Draw();
	}
}

//=============================================================================================//
// 終了処理

void SceneManager::Finalize() {
	pendingScene_.reset();

	ReleaseScene(currentScene_);

	ReleaseScene(editScene_);

	playing_ = false;

	restoreEditScene_ = false;
}

//=============================================================================================//
// Getter / 現在の場面

IEditableWorld* SceneManager::GetEditableWorld() {
	if (currentScene_ == nullptr) {
		return nullptr;
	}

	return currentScene_->GetEditableWorld();
}

const char* SceneManager::GetSceneName() const {
	if (currentScene_ == nullptr) {
		return "Scene";
	}

	return currentScene_->GetName();
}

//=============================================================================================//
// Setter / 再生状態

void SceneManager::SetPlayState(bool playing) {
	playing_ = playing;

	if (!playing) {
		pendingScene_.reset();

		if (editScene_ != nullptr) {
			restoreEditScene_ = true;
		}
	}

	if (currentScene_ != nullptr) {
		currentScene_->OnPlayStateChanged(playing);
	}
}

//=============================================================================================//
// 終了・所有権の解放

void SceneManager::ReleaseScene(std::unique_ptr<IScene>& scene) {
	if (scene != nullptr) {
		scene->Finalize();

		scene.reset();
	}
}
