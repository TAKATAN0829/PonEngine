#include "SpriteManager.h"

//=================================================================================================//
// 初期化処理

void SpriteManager::Initialize(int32_t clientWidth, int32_t clientHeight) {

	//=============================================================================================//
	// ClientWidth保存

	clientWidth_ = clientWidth;

	//=============================================================================================//
	// ClientHeight保存

	clientHeight_ = clientHeight;
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

void SpriteManager::Draw() {

	//=============================================================================================//
	// Sprite描画
	ID3D12GraphicsCommandList* commandList = GraphicsSystem::GetCommandList();
	GraphicsSystem* graphicsSystem = GraphicsSystem::GetGraphicsSystem();

	for (Sprite* sprite : sprites_) {

		sprite->Draw();
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

	Sprite* sprite = new Sprite();

	sprite->Initialize(clientWidth_, clientHeight_);

	//=============================================================================================//
	// 配列追加

	sprites_.push_back(sprite);

	return sprite;
}