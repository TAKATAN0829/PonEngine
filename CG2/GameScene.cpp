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
}

//=================================================================================================//
// 描画処理

void GameScene::Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	//=============================================================================================//
	// ObjectManager描画

	objectManager_->Draw (commandList, graphicsSystem);
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