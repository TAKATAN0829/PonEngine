#pragma once

// 自作
#include "IScene.h"
#include "IEditableScene.h"
#include "Scene.h"
#include "EngineStructs.h"
#include "Camera.h"
#include "Particle.h"
#include "DebugCamera.h"
#include "Object3dCommon.h"
#include "Sprite.h"
#include "SpriteCommon.h"

class GameScene : public IScene, public IEditableScene {

public:

	//===========================================//
	// 初期化処理
	//===========================================//

	void Initialize (const SceneContext& context) override;

	//===========================================//
	// 更新処理
	//===========================================//

	void Update (Input* input) override;

	//===========================================//
	// 描画処理
	//===========================================//

	void Draw () override;

	//===========================================//
	// 終了処理
	//===========================================//

	void Finalize () override;

	//===========================================//
	// 編集対象の取得
	//===========================================//

	IEditableScene* GetEditableScene() override;
	size_t GetEditableObjectCount() const override;
	EditableObjectView GetEditableObject(size_t index) override;

	//===========================================//
	// Camera更新
	//===========================================//

	void UpdateCamera (Input* input);

	//===========================================//
	// Object更新
	//===========================================//

	void UpdateObject ();

private:

	//===========================================//
	// Scene
	//===========================================//

	Scene* scene_;

	//===========================================//
	// Camera
	//===========================================//

	Camera* camera_;
	TransformData cameraTransform_;

	//===========================================//
	// DebugCamera
	//===========================================//

	DebugCamera* debugCamera_;

	//===========================================//
	// Object3dCommon
	//===========================================//

	Object3dCommon* object3dCommon_;

	//===========================================//
	// モデル
	//===========================================//

	//==================//
	// メインモデル

	ModelId playerId_;
	int modelIndex_; // モデルを切り替えるための番号

	//==================//
	// サブモデル

	ModelId subModelId_;

	//===========================================//
	// Particle
	//===========================================//

	Particle* particle_;
	Vector4 color_;
	BlendMode particleBlendMode_;
	bool useBillboard_;
	bool useAccelerationField_;

	//===========================================//
	// Sprite
	//===========================================//

	SpriteCommon* spriteCommon_;
	Sprite* sprite_;
	Vector2 spritePosition_;
	float spriteRotation_;
	Vector2 spriteSize_;

	//===========================================//
	// 平行光源
	//===========================================//

	Vector4 lightColor_;
	Vector3 lightDirection_;
	float lightIntensity_;

	//===========================================//
	// 点光源
	//===========================================//

	PointLight pointLight_;

	//===========================================//
	// スポットライト
	//===========================================//

	SpotLight spotLight_;
};
