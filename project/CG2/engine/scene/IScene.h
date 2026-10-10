#pragma once

// 自作
#include "Input.h"

class Object3dRenderer;
class SpriteCommon;
class ParticleRenderer;

//=================================================================================================//
// シーンに渡す描画機能

struct SceneContext {

	Object3dRenderer* object3dRenderer = nullptr;

	SpriteCommon* spriteCommon = nullptr;

	ParticleRenderer* particleRenderer = nullptr;
};

//=================================================================================================//
// シーン基底クラス

class IScene {
public:

	virtual ~IScene() = default;

	//=============================================================================================//
	// 初期化処理

	virtual void Initialize(const SceneContext& context) = 0;

	//=============================================================================================//
	// 更新処理

	virtual void Update(Input* input) = 0;

	//=============================================================================================//
	// 描画処理

	virtual void Draw() = 0;

	//=============================================================================================//
	// 終了処理

	virtual void Finalize() = 0;
};
