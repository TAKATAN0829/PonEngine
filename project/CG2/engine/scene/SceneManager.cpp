#include "SceneManager.h"

// C++
#include <cassert>

//=================================================================================================//
// 初期化処理

void SceneManager::Initialize(const SceneContext& context, IScene* firstScene) {

	assert(firstScene);

	//===========================================//
	// モデル一覧初期化
	//===========================================//

	modelCollection_.Initialize();

	//===========================================//
	// モデル描画初期化
	//===========================================//

	modelRenderer_.Initialize(context.object3dPipeline);

	// ModelCollection本体はコピーせず、同じ実物の場所を渡す。
	SceneContext sceneContext = context;
	sceneContext.modelCollection = &modelCollection_;
	sceneContext.modelRenderer = &modelRenderer_;

	// 最初のシーン設定
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
// モデル一覧の取得

ModelCollection* SceneManager::GetModelCollection() {
	return &modelCollection_;
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
	// モデル描画解放
	//===========================================//

	modelRenderer_.Finalize();

	//===========================================//
	// モデル一覧解放
	//===========================================//

	modelCollection_.Finalize();
}
