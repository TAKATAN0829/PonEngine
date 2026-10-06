#pragma once

// C++
#include <vector>

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
	// 三角形

	std::vector<Object3d*> triangles_;
	std::vector<TransformData> trianglesTransformData_;

	std::vector<Object3d*> invTriangles_;
	std::vector<TransformData> invTrianglesTransformData_;

	// 三角形一個当たりのトランスフォームデータ
	TransformData triangleTransform_;
	TransformData invTriangleTransform_;

	//=============================================================================================//
	// 表示する三角形数

	int triangleCountsX_ = 14;
	int triangleCountsY_ = 8;

	//=============================================================================================//
	// 三角形の色とBlendMode

	Vector4 triangleColor_ = { 1.0f,1.0f,1.0f,1.0f };

	BlendMode triangleBlendMode_ = kBlendModeNormal;

	//=============================================================================================//
	// Particle

	Particle* particle_ = nullptr;

	//=============================================================================================//
	// DebugCamera

	DebugCamera* debugCamera_ = nullptr;
};