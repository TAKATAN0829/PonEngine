#pragma once

// C++
#include <vector>

// DirectX
#include <d3d12.h>

// 自作
#include "Sprite.h"
#include "GraphicsSystem.h"
#include "ResourceManager.h"

class SpriteManager {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(
		ID3D12Device* device,
		ResourceManager* resourceManager,
		int32_t clientWidth,
		int32_t clientHeight);

	//=============================================================================================//
	// 更新処理

	void Update();

	//=============================================================================================//
	// 描画処理

	void Draw(ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem);

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Sprite生成

	Sprite* CreateSprite();

private:

	//=============================================================================================//
	// Device

	ID3D12Device* device_ =	nullptr;

	//=============================================================================================//
	// ResourceManager

	ResourceManager* resourceManager_ = nullptr;

	//=============================================================================================//
	// ClientWidth

	int32_t clientWidth_ = 0;

	//=============================================================================================//
	// ClientHeight

	int32_t clientHeight_ = 0;

	//=============================================================================================//
	// Sprites

	std::vector<Sprite*> sprites_;
};
