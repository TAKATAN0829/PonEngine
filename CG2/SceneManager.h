#pragma once

// DirectX
#include <d3d12.h>

// 自作
#include "IScene.h"
#include "TitleScene.h"
#include "GameScene.h"
#include "Input.h"
#include "GraphicsSystem.h"
#include "ResourceManager.h"

class SceneManager {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (
		ID3D12Device* device,
		ResourceManager* resourceManager,
		int32_t clientWidth,
		int32_t clientHeight);

	//=============================================================================================//
	// 更新処理

	void Update (
		Input* input);

	//=============================================================================================//
	// 描画処理

	void Draw (
		ID3D12GraphicsCommandList* commandList,
		GraphicsSystem* graphicsSystem);

	//=============================================================================================//
	// 終了処理

	void Finalize ();

private:

	//=============================================================================================//
	// 現在のシーン

	IScene* currentScene_ = nullptr;

	//=============================================================================================//
	// Device

	ID3D12Device* device_ = nullptr;

	//=============================================================================================//
	// ResourceManager

	ResourceManager* resourceManager_ = nullptr;

	//=============================================================================================//
	// 画面幅

	int32_t clientWidth_ = 0;

	//=============================================================================================//
	// 画面高さ

	int32_t clientHeight_ = 0;
};