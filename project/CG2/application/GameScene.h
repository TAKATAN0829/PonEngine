#pragma once

// 自作
#include "IScene.h"
#include "SceneObjects.h"
#include <memory>
#include <unordered_map>
#include "EngineStructs.h"
#include "Camera.h"
#include "Particle.h"
#include "DebugCamera.h"
#include "Object3dCommon.h"
#include "Object3d.h"
#include "Sprite.h"
#include "SpriteCommon.h"

class GameScene : public IScene, public SceneObjects {
public:

	void Initialize (const SceneContext& context) override;
	void Update (Input* input) override;
	void Draw () override;
	void Finalize () override;
	void UpdateEditorFrame(Input* input, bool simulate) override;
	void OnPlayStateChanged(bool playing) override;

	IEditableScene* GetEditableScene() override { return this; }
	bool GetEditorViewInfo(EditorViewInfo& viewInfo) const override;
	bool GetEditableObjectBounds(size_t objectIndex, AABB& worldBounds) const override;
	void FocusEditorCamera(const Vector3& position) override;
	void SetEditorViewport(uint32_t width, uint32_t height) override;
	void SetEditorCameraInput(bool enabled) override;
	void SetEditorGameView(bool enabled) override { gameView_ = enabled; }
	ParticleSettings* GetParticleSettings() override { return &particleSettings_; }
	void SetParticlePreview(const ParticlePreviewState& state) override { particlePreview_ = state; }
	void ResetParticlePreview() override;
	void BurstParticles() override { particleBurstRequested_ = true; }
	size_t GetParticleCount() const override;



	void UpdateCamera (Input* input);
	void UpdateObject ();

private:

	Camera* camera_ = nullptr;
	TransformData cameraTransform_{};

	//=============================================================================================//
	// 色



	//=============================================================================================//
	// Particle

	Particle* particle_ = nullptr;



	//=============================================================================================//
	// DebugCamera

	DebugCamera* debugCamera_ = nullptr;

	//=============================================================================================//
	// Object3dCommon

	Object3dCommon* object3dCommon_ = nullptr;

	//=============================================================================================//
	// ビルボード



	//=============================================================================================//
	// 加速度Field



	//=============================================================================================//
	// モデル

	// 編集データのIDに対応する描画用リソース。保存ファイルや履歴には含めない。
	struct RuntimeObject {
		std::unique_ptr<Object3d> drawable;
		std::string model;
	};
	std::unordered_map<SceneObjectId, RuntimeObject> runtimeObjects_;
	bool simulationEnabled_ = true; // このフレームに時間経過を進めるかどうか。
	bool playing_ = false; // Pause中もtrueとなる、再生モード自体の状態。
	ParticleSettings particleSettings_;
	ParticlePreviewState particlePreview_;
	bool particleBurstRequested_ = false;
	bool gameView_ = false;
	uint32_t viewportWidth_ = 1600;
	uint32_t viewportHeight_ = 900;
	//=============================================================================================//
	// SpriteCommon

	SpriteCommon* spriteCommon_ = nullptr;

	//=============================================================================================//
	// Sprite

	Sprite* sprite_ = nullptr;

	Vector2 spritePosition_ = { 100.f,100.f };

	float spriteRotation_ = 0.f;

	Vector2 spriteSize_ = { 256.f,256.f };

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
