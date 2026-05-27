#include "GameScene.h"

//=================================================================================================//
// 初期化処理

void GameScene::Initialize () {

	//=============================================================================================//
	// ObjectManager初期化

	objectManager_ = new ObjectManager ();

	objectManager_->Initialize (device_, resourceManager_);

	//=============================================================================================//
	// Camera初期化

	camera_ = new Camera ();

	camera_->Initialize (clientWidth_, clientHeight_);

	//=============================================================================================//
	// Object生成

	Object3d* objectA = objectManager_->CreateObject (MeshType::kSphere);

	TransformData transformA{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{-1.5f,0.0f,0.0f}
	};

	objectA->SetTransform (transformA);

	objectA->SetTextureIndex (0);

	Object3d* objectB = objectManager_->CreateObject (MeshType::kSphere);

	TransformData transformB{
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{1.5f,0.0f,0.0f}
	};

	objectB->SetTransform (transformB);

	objectB->SetTextureIndex (0);

	//=============================================================================================//
	// Sprite生成

	sprite_ =new Sprite();

	sprite_->Initialize(
		device_,
		resourceManager_,
		clientWidth_,
		clientHeight_);

	sprite_->SetTextureIndex(0);

	sprite_->SetPosition({ 640.0f,360.0f });

	sprite_->SetSize({ 200.0f,200.0f });

	//=============================================================================================//
	// SpriteManager生成

	spriteManager_ = new SpriteManager();

	spriteManager_->Initialize(
		device_,
		resourceManager_,
		clientWidth_,
		clientHeight_);
}

//=================================================================================================//
// 更新処理

void GameScene::Update (Input* input) {

	input;

	//=============================================================================================//
	// Camera更新

	camera_->Update ();

	//=============================================================================================//
	// ObjectManager更新

	objectManager_->Update (camera_->GetViewProjectionMatrix ());

	//=============================================================================================//
	// Sprite更新

	sprite_->Update();
}

//=================================================================================================//
// 描画処理

void GameScene::Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	//=============================================================================================//
	// ObjectManager描画

	objectManager_->Draw (commandList, graphicsSystem);

	//=============================================================================================//
	// Sprite描画

	sprite_->Draw(commandList, graphicsSystem);

	//=============================================================================================//
	// SpriteManager描画

	spriteManager_->Draw(commandList, graphicsSystem);
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize () {

	//=============================================================================================//
	// ObjectManager解放

	if (objectManager_ != nullptr) {

		objectManager_->Finalize ();

		delete objectManager_;

		objectManager_ = nullptr;
	}

	//=============================================================================================//
	// Sprite解放

	if (sprite_ != nullptr) {

		delete sprite_;

		sprite_ = nullptr;
	}

	//=============================================================================================//
	// SpriteManager解放

	if (spriteManager_ != nullptr) {

		spriteManager_->Finalize();

		delete spriteManager_;

		spriteManager_ = nullptr;
	}

	//=============================================================================================//
	// Camera解放

	if (camera_ != nullptr) {

		delete camera_;

		camera_ = nullptr;
	}
}

//=================================================================================================//
// ResourceManager設定

void GameScene::SetResourceManager (ResourceManager* resourceManager) {

	resourceManager_ = resourceManager;
}

//=================================================================================================//
// Device設定

void GameScene::SetDevice (ID3D12Device* device) {

	device_ = device;
}

//=================================================================================================//
// 画面サイズ設定

void GameScene::SetWindowSize (int32_t width, int32_t height) {

	clientWidth_ = width;

	clientHeight_ = height;
}

//=================================================================================================//
// 終了判定

bool GameScene::IsFinished () {

	return isFinished_;
}

//=================================================================================================//
// シーン共通情報

void GameScene::SetContext(const SceneContext& sceneContext) {

	device_ = sceneContext.device;

	resourceManager_ = sceneContext.resourceManager;

	clientWidth_ = sceneContext.clientWidth;

	clientHeight_ = sceneContext.clientHeight;
}