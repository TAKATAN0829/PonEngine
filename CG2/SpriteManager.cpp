#include "SpriteManager.h"

//=================================================================================================//
// 初期化処理

void SpriteManager::Initialize(
	ID3D12Device* device,
	ResourceManager* resourceManager,
	int32_t clientWidth,
	int32_t clientHeight) {

	//=============================================================================================//
	// Device保存

	device_ = device;

	//=============================================================================================//
	// ResourceManager保存

	resourceManager_ = resourceManager;

	//=============================================================================================//
	// ClientWidth保存

	clientWidth_ = clientWidth;

	//=============================================================================================//
	// ClientHeight保存

	clientHeight_ =	clientHeight;
}

//=================================================================================================//
// 更新処理

void SpriteManager::Update() {

	//=============================================================================================//
	// Sprite更新

	for (Sprite* sprite : sprites_) {

		sprite->Update();
	}
}

//=================================================================================================//
// 描画処理

void SpriteManager::Draw(ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	//=============================================================================================//
	// Sprite描画

	for (Sprite* sprite : sprites_) {

		sprite->Draw(commandList, graphicsSystem);
	}
}

//=================================================================================================//
// 終了処理

void SpriteManager::Finalize() {

	//=============================================================================================//
	// Sprite解放

	for (Sprite* sprite : sprites_) {

		if (sprite != nullptr) {

			sprite->Finalize();

			delete sprite;

			sprite = nullptr;
		}
	}

	sprites_.clear();
}

//=================================================================================================//
// Sprite生成

Sprite* SpriteManager::CreateSprite() {

	//=============================================================================================//
	// Sprite生成

	Sprite* sprite =new Sprite();

	sprite->Initialize(
		device_,
		resourceManager_,
		clientWidth_,
		clientHeight_);

	//=============================================================================================//
	// 配列追加

	sprites_.push_back(sprite);

	return sprite;
}