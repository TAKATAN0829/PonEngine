#include "SceneManager.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void SceneManager::Initialize(const SceneContext& context, IScene* firstScene) {

	assert(firstScene);

	//===========================================//
	// シーンデータ初期化
	//===========================================//

	scene_.Initialize(context.object3dCommon);

	// Scene本体はコピーせず、同じ実物の場所を渡す。
	SceneContext sceneContext = context;
	sceneContext.scene = &scene_;

	// 最初のScene設定
	currentScene_ = firstScene;
	currentScene_->Initialize(sceneContext);
}

//=================================================================================================//
// 更新処理

void SceneManager::Update(Input* input) {

	//===========================================//
	// CurrentScene更新
	//===========================================//

	currentScene_->Update(input);
}

//=================================================================================================//
// 描画処理

void SceneManager::Draw() {

	//===========================================//
	// CurrentScene描画
	//===========================================//

	currentScene_->Draw();
}

//=================================================================================================//
// 編集対象の取得

IEditableScene* SceneManager::GetEditableScene() {
	if (currentScene_ != nullptr) {
		return currentScene_->GetEditableScene();
	}

	return nullptr;
}

//=================================================================================================//
// 終了処理

void SceneManager::Finalize() {

	//===========================================//
	// CurrentScene解放
	//===========================================//

	if (currentScene_ != nullptr) {

		currentScene_->Finalize();

		delete currentScene_;

		currentScene_ = nullptr;
	}

	//===========================================//
	// シーンデータ解放
	//===========================================//

	scene_.Finalize();
}
