#pragma once

// 自作
#include "IScene.h"
#include "EngineStructs.h"
#include "Camera.h"
#include "Object3d.h"
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
	// 色とBlendMode

	Vector4 color_ = { 1.0f,1.0f,1.0f,1.0f };

	BlendMode blendMode_ = kBlendModeNormal;

	//=============================================================================================//
	// Particle

	Particle* particle_ = nullptr;

	//=============================================================================================//
	// Fence

	Object3d* fence_ = nullptr;

	TransformData fenceTransform_ = {
		{3.0f,3.0f,3.0f},
		{0.0f,3.14159265f,0.0f},
		{0.0f,0.0f,-15.0f}
	};

	//=============================================================================================//
	// DebugCamera

	DebugCamera* debugCamera_ = nullptr;
};