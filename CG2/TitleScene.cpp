#include "TitleScene.h"


//=================================================================================================//
// SceneContext設定

void TitleScene::SetContext(const SceneContext& sceneContext) {

	device_ = sceneContext.device;

	resourceManager_ = sceneContext.resourceManager;

	clientWidth_ = sceneContext.clientWidth;

	clientHeight_ = sceneContext.clientHeight;
}

//=================================================================================================//
// 初期化処理

void TitleScene::Initialize() {

	//=============================================================================================//
	// SpriteManager生成

	spriteManager_ = new SpriteManager();

	spriteManager_->Initialize(
		device_,
		resourceManager_,
		clientWidth_,
		clientHeight_);

	//=============================================================================================//
	// BackGroundSprite生成

	backGroundSprite_ =	spriteManager_->CreateSprite();

	backGroundSprite_->SetTextureIndex(0);

	backGroundSprite_->SetPosition({ 640.0f,360.0f });

	backGroundSprite_->SetSize({ 512.0f,512.0f });

	backGroundSprite_->SetEnableLighting(false);
}

//=================================================================================================//
// 更新処理

void TitleScene::Update(Input* input) {

	//=============================================================================================//
	// Enterで終了

	if (input->IsTriggerKey(DIK_RETURN)) {

		isFinished_ = true;
	}

	//=============================================================================================//
	// SpriteManager更新

	spriteManager_->Update();
}

//=================================================================================================//
// 描画処理

void TitleScene::Draw(ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	//=============================================================================================//
	// SpriteManager描画

	spriteManager_->Draw(commandList, graphicsSystem);
}

//=================================================================================================//
// 終了処理

void TitleScene::Finalize() {

	//=============================================================================================//
	// SpriteManager解放

	if (spriteManager_ != nullptr) {

		spriteManager_->Finalize();

		delete spriteManager_;

		spriteManager_ = nullptr;
	}
}

//=================================================================================================//
// 終了判定

bool TitleScene::IsFinished() {

	return isFinished_;
}