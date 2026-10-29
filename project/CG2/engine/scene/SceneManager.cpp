#include "SceneManager.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void SceneManager::Initialize(const SceneContext& context, IScene* firstScene) {

	assert(firstScene);

	//=============================================================================================//
	// 最初のScene設定

	currentScene_ = firstScene;

	currentScene_->Initialize(context);
}

//=================================================================================================//
// 更新処理

void SceneManager::Update(Input* input) {

	//=============================================================================================//
	// CurrentScene更新

	currentScene_->Update(input);
}

//=================================================================================================//
// 描画処理

void SceneManager::Draw() {

	//=============================================================================================//
	// CurrentScene描画

	currentScene_->Draw();
}

//=================================================================================================//
// 終了処理

void SceneManager::Finalize() {

	//=============================================================================================//
	// CurrentScene解放

	if (currentScene_ != nullptr) {

		currentScene_->Finalize();

		delete currentScene_;

		currentScene_ = nullptr;
	}
}
