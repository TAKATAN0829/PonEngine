#pragma once

// 自作
#include "IScene.h"
#include "EngineStructs.h"
#include "Camera.h"
#include "Particle.h"
#include "DebugCamera.h"

class GameScene : public IScene {
public:

	void Initialize (const SceneContext& context) override;
	void Update (Input* input) override;
	void Draw () override;
	void Finalize () override;

	void UpdateCamera (Input* input);
	void UpdateObject ();

private:

	Camera* camera_ = nullptr;
	TransformData cameraTransform_{};

	//=============================================================================================//
	// 色

	Vector4 color_ = { 1.0f,1.0f,1.0f,1.0f };

	//=============================================================================================//
	// Particle

	Particle* particle_ = nullptr;

	BlendMode particleBlendMode_ = kBlendModeAdd;

	//=============================================================================================//
	// DebugCamera

	DebugCamera* debugCamera_ = nullptr;

	//=============================================================================================//
	// ビルボード

	bool useBillboard_ = true;

	//=============================================================================================//
	// 加速度Field

	bool useAccelerationField_ = false;
};
