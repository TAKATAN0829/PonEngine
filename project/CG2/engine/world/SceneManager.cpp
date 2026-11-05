#include "SceneManager.h"
#include <cassert>
#include <utility>

//=============================================================================================//
// 初期化処理

void SceneManager::Initialize(const SceneContext& context, IScene* firstScene) {
	// 渡された最初のシーンを所有し、描画機能を渡して初期化する。
	assert(firstScene != nullptr);

	assert(currentScene_ == nullptr);

	context_ = context;

	currentScene_.reset(firstScene);

	currentScene_->Initialize(context_);
}

//=============================================================================================//
// 更新処理

void SceneManager::Update(Input* input, bool shouldSimulate) {
	// 現在の場面を更新し、再生状態と遷移要求を確認する。
	if (currentScene_ == nullptr) {
		return;
	}

	currentScene_->UpdateEditorFrame(input, shouldSimulate);

	if (pendingScene_ == nullptr) {
		pendingScene_ = currentScene_->TakeNextScene();
	}
}

bool SceneManager::ApplyPendingScene() {
	if (shouldRestoreEditScene_) {
		// 停止と同じフレームに遷移要求があっても、元の場面への復帰を優先する。
		pendingScene_.reset();

		ReleaseScene(currentScene_);

		currentScene_ = std::move(editScene_);

		shouldRestoreEditScene_ = false;

		currentScene_->OnPlayStateChanged(false);

		return true;
	}

	if (pendingScene_ == nullptr) {
		return false;
	}

	if (isPlaying_) {
		if (editScene_ == nullptr) {
			// EditorSessionの停止前スナップショットが参照する場面を保持する。
			currentScene_->OnPlayStateChanged(false);

			editScene_ = std::move(currentScene_);
		}
	}

	ReleaseScene(currentScene_);

	currentScene_ = std::move(pendingScene_);

	currentScene_->Initialize(context_);

	if (isPlaying_) {
		currentScene_->OnPlayStateChanged(true);
	}

	return true;
}

//=============================================================================================//
// 描画処理

void SceneManager::Draw() {
	// 現在のシーンがある場合だけ描画を呼ぶ。
	if (currentScene_ != nullptr) {
		currentScene_->Draw();
	}
}

//=============================================================================================//
// 終了処理

void SceneManager::Finalize() {
	// 実行中・保留中・再生開始時のシーンを終了する。
	pendingScene_.reset();

	ReleaseScene(currentScene_);

	ReleaseScene(editScene_);

	isPlaying_ = false;

	shouldRestoreEditScene_ = false;
}

//=============================================================================================//
// Getter / 現在の場面

IEditableWorld* SceneManager::GetEditableWorld() {
	// 現在のシーンが公開する編集用の窓口を取得する。
	if (currentScene_ == nullptr) {
		return nullptr;
	}

	return currentScene_->GetEditableWorld();
}

const char* SceneManager::GetSceneName() const {
	// 現在の場面名を取得する。
	if (currentScene_ == nullptr) {
		return "Scene";
	}

	return currentScene_->GetName();
}

std::string SceneManager::GetInitialAssetPath() const {
	// 起動時に読み込んだシーンファイルのパスを取得する。
	if (currentScene_ == nullptr) {
		return {};
	}

	return currentScene_->GetInitialAssetPath();
}

//=============================================================================================//
// Setter / 再生状態

void SceneManager::SetPlayState(bool isPlaying) {
	// 再生開始時の編集シーンを保持し、停止時の復帰を準備する。
	isPlaying_ = isPlaying;

	if (!isPlaying) {
		pendingScene_.reset();

		if (editScene_ != nullptr) {
			shouldRestoreEditScene_ = true;
		}
	}

	if (currentScene_ != nullptr) {
		currentScene_->OnPlayStateChanged(isPlaying);
	}
}

//=============================================================================================//
// 終了・所有権の解放

void SceneManager::ReleaseScene(std::unique_ptr<IScene>& scene) {
	// Finalizeを呼んでから、所有しているシーンを破棄する。
	if (scene != nullptr) {
		scene->Finalize();

		scene.reset();
	}
}
