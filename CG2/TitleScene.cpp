#include "TitleScene.h"

//=================================================================================================//
// 初期化処理

void TitleScene::Initialize () {

	isFinished_ = false;
}

//=================================================================================================//
// 更新処理

void TitleScene::Update (Input* input) {

	// SPACEでGameSceneへ
	if (input->IsTriggerKey(DIK_SPACE)) {

		isFinished_ = true;
	}
}

//=================================================================================================//
// 描画処理

void TitleScene::Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	commandList;
	graphicsSystem;
}

//=================================================================================================//
// 終了処理

void TitleScene::Finalize () {
}

//=================================================================================================//
// 終了判定

bool TitleScene::IsFinished () {

	return isFinished_;
}