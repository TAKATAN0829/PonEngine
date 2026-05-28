#include "GameScene.h"

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

//=================================================================================================//
// 初期化処理

void GameScene::Initialize () {

	//=============================================================================================//
	// Camera初期化

	camera_ = new Camera ();

	camera_->Initialize (clientWidth_, clientHeight_);

	//=============================================================================================//
	// 三角形初期化

	triangle_ = new Object3d ();

	triangle_->Initialize (device_, resourceManager_, MeshType::kTriangle);

	triangleTransformData_.scale = { 1.0f,1.0f,1.0f };

	triangleTransformData_.rotate = { 0.0f,0.0f,0.0f };

	triangleTransformData_.translate = { 0.0f,0.0f,5.0f };

	triangle_->SetTransform (triangleTransformData_);

	triangle_->SetColor ({ 1.0f,1.0f,1.0f,1.0f });
}

//=================================================================================================//
// 更新処理

void GameScene::Update (Input* input) {

	input;

#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::Begin ("Triangle");

	ImGui::DragFloat3 ("Rotate", &triangleTransformData_.rotate.x, 0.1f, -100.f, 100.f);

	ImGui::End ();

#endif

	//=============================================================================================//
	// Camera更新

	camera_->Update ();

	//=============================================================================================//
	// 三角形更新

	triangle_->SetTransform (triangleTransformData_);

	triangle_->Update (camera_->GetViewProjectionMatrix ());
}

//=================================================================================================//
// 描画処理

void GameScene::Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	//=============================================================================================//
	// 三角形描画

	triangle_->Draw (commandList, graphicsSystem);
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize () {

	//=============================================================================================//
	// 三角形解放

	if (triangle_ != nullptr) {

		triangle_->Finalize ();

		delete triangle_;

		triangle_ = nullptr;
	}

	//=============================================================================================//
	// Camera解放

	if (camera_ != nullptr) {

		delete camera_;

		camera_ = nullptr;
	}
}

//=================================================================================================//
// ResourceManager設定

void GameScene::SetResourceManager (ResourceManager* resourceManager) {

	resourceManager_ = resourceManager;
}

//=================================================================================================//
// Device設定

void GameScene::SetDevice (ID3D12Device* device) {

	device_ = device;
}

//=================================================================================================//
// 画面サイズ設定

void GameScene::SetWindowSize (int32_t width, int32_t height) {

	clientWidth_ = width;

	clientHeight_ = height;
}

//=================================================================================================//
// 終了判定

bool GameScene::IsFinished () {

	return isFinished_;
}

//=================================================================================================//
// シーン共通情報

void GameScene::SetContext (const SceneContext& sceneContext) {

	device_ = sceneContext.device;

	resourceManager_ = sceneContext.resourceManager;

	clientWidth_ = sceneContext.clientWidth;

	clientHeight_ = sceneContext.clientHeight;
}