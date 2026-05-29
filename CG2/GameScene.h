#pragma once

// C++
#include <vector>

// 自作
#include "IScene.h"
#include "EngineStructs.h"
#include "ResourceManager.h"
#include "Camera.h"
#include "Object3d.h"

class GameScene : public IScene {
public:

	void Initialize() override;
	void Update(Input* input) override;
	void Draw() override;
	void Finalize() override;

	void SetResourceManager(ResourceManager* resourceManager);
	void SetDevice(ID3D12Device* device);
	void SetWindowSize(int32_t width, int32_t height);

	bool IsFinished() override;
	void SetContext(const SceneContext& sceneContext) override;

private:

	ID3D12Device* device_ = nullptr;
	ResourceManager* resourceManager_ = nullptr;

	Camera* camera_ = nullptr;
	TransformData cameraTransform_{};

	int32_t clientWidth_ = 0;
	int32_t clientHeight_ = 0;

	bool isFinished_ = false;

	//=============================================================================================//
	// 三角形

	std::vector<Object3d*> triangles_;
	std::vector<TransformData> trianglesTransformData_;

	std::vector<Object3d*> invTriangles_;
	std::vector<TransformData> invTrianglesTransformData_;

	//=============================================================================================//
	// 表示する三角形数

	int triangleCountsX_ = 14;
	int triangleCountsY_ = 8;
};