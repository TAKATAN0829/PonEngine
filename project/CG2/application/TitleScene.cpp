#include "TitleScene.h"
#include "GameScene.h"
#include "objects/WorldPrimitives.h"
#include "LogSystem.h"

//=============================================================================================//
// 初期化処理

void TitleScene::Initialize(const SceneContext& context) {
	WorldScene::Initialize(context);

	// 保存済みのゲーム配置を読まず、この場面専用のCubeとカメラを用意する。
	WorldObjectData titleCube;

	titleCube.id = 1;

	titleCube.name = "Title Cube - Enter to start";

	titleCube.model = WorldPrimitives::kCubeModel;

	titleCube.color = {0.2f, 0.65f, 1, 1};

	titleCube.enableLighting = false;

	titleCube.transform.rotate = {0.3f, 0, 0};

	titleCube.motion.hasRotation = true;

	titleCube.motion.rotationDegreesPerSecond = {0, 30, 0};

	WorldObjectData titleCamera;

	titleCamera.id = 2;

	titleCamera.name = "Title Camera";

	titleCamera.type = WorldObjectType::Camera;

	titleCamera.hasMeshRenderer = false;

	titleCamera.mainCamera = true;

	titleCamera.transform.translate = {0, 0, -4};

	std::string error;

	if (!ReplaceObjects({titleCube, titleCamera}, error)) {
		LogSystem::Log("Title scene: " + error + "\n");
	}

	InitializeSimulation();

	LogSystem::Log("[TitleScene] Play / Enter: GameScene. GameScene / Escape: TitleScene.\n");
}

//=============================================================================================//
// 更新処理

void TitleScene::Update(Input* input) {
	if (input != nullptr) {
		if (CanHandleGameInput()) {
			if (input->IsTriggerKey(DIK_RETURN)) {
				RequestSceneChange(std::make_unique<GameScene>());
			}
		}
	}

	WorldScene::Update(input);
}
