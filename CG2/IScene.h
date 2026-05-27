#pragma once

// DirectX
#include <d3d12.h>

// C++
#include <cstdint>

// 自作
#include "GraphicsSystem.h"
#include "Input.h"

//=================================================================================================//
// SceneContext

struct SceneContext {
	ID3D12Device* device = nullptr;

	ResourceManager* resourceManager = nullptr;

	int32_t clientWidth = 0;

	int32_t clientHeight = 0;
};

//=================================================================================================//
// シーン基底クラス

class IScene {
public:

	virtual ~IScene() = default;

	//=============================================================================================//
	// SceneContext設定

	virtual void SetContext(const SceneContext& sceneContext) = 0;

	//=============================================================================================//
	// 初期化処理

	virtual void Initialize() = 0;

	//=============================================================================================//
	// 更新処理

	virtual void Update(Input* input) = 0;

	//=============================================================================================//
	// 描画処理

	virtual void Draw(ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) = 0;

	//=============================================================================================//
	// 終了処理

	virtual void Finalize() = 0;

	//=============================================================================================//
	// 終了判定

	virtual bool IsFinished() = 0;
};