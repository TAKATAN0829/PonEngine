#pragma once

// 自作
#include "Input.h"

class ModelCollection;
class ModelRenderer;
class Object3dPipeline;
class SpriteCommon;
class ParticleRenderer;

//=================================================================================================//
// シーンに渡す描画機能

struct SceneContext {

	ModelCollection* modelCollection;
	ModelRenderer* modelRenderer;

	Object3dPipeline* object3dPipeline = nullptr;

	SpriteCommon* spriteCommon = nullptr;

	ParticleRenderer* particleRenderer = nullptr;
};

//=================================================================================================//
// シーン基底クラス

class IScene {
public:

	virtual ~IScene() = default;

	//===========================================//
	// 初期化処理
	//===========================================//

	virtual void Initialize(const SceneContext& context) = 0;

	//===========================================//
	// 更新処理
	//===========================================//

	virtual void Update(Input* input) = 0;

	//===========================================//
	// 描画処理
	//===========================================//

	virtual void Draw() = 0;

	//===========================================//
	// 終了処理
	//===========================================//

	virtual void Finalize() = 0;
};
