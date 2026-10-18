#pragma once

// 自作
#include "IScene.h"
#include "EngineStructs.h"
#include "Camera.h"
#include "Particle.h"
#include "DebugCamera.h"
#include "Object3dRenderer.h"
#include "Object3d.h"

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
	// Object3dRenderer

	Object3dRenderer* object3dRenderer_ = nullptr;

	//=============================================================================================//
	// ビルボード

	bool useBillboard_ = true;

	//=============================================================================================//
	// 加速度Field

	bool useAccelerationField_ = false;

	//=============================================================================================//
	// モデル

	Object3d* model_ = nullptr;

	TransformData modelTransform_ = {
		{ 5.0f,5.0f,5.0f },
		{ 0.0f,0.0f,0.0f },
		{ 0.0f,0.0f,0.0f }
	};

	Vector4 modelColor_ = { 1.0f,1.0f,1.0f,1.0f };

	bool enableLighting_ = true;

	float shininess_ = 40.0f;

	//=============================================================================================//
	// 平行光源

	Vector4 lightColor_ = { 1.0f,1.0f,1.0f,1.0f };

	Vector3 lightDirection_ = { 0.0f,-1.0f,0.0f };

	float lightIntensity_ = 1.0f;

	//=============================================================================================//
	// 点光源

	PointLight pointLight_ = {
		{ 1.0f,1.0f,1.0f,1.0f },
		{ 0.0f,2.0f,0.0f },
		1.0f,
		10.0f,
		1.0f
	};

	//=============================================================================================//
	// スポットライト

	SpotLight spotLight_ = {
		{ 1.0f,1.0f,1.0f,1.0f },
		{ 2.0f,1.25f,0.0f },
		4.0f,
		{ -1.0f,-1.0f,0.0f },
		7.0f,
		2.0f,
		0.5f,
		1.0f
	};
};
