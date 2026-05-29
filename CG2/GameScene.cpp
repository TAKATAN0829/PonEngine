#include "GameScene.h"

#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#endif

//=================================================================================================//
// 初期化処理

void GameScene::Initialize() {

	//=============================================================================================//
	// Camera初期化

	camera_ = new Camera();
	camera_->Initialize(clientWidth_, clientHeight_);
	cameraTransform_ = camera_->GetTransform();

	//=============================================================================================//
	// 三角形初期化

	for (int count = 0; count < kTriangleCounts_; ++count) {

		Object3d* triangle = new Object3d();

		triangle->Initialize(device_, resourceManager_, MeshType::kTriangle);

		TransformData transform{};

		transform.scale = { 1.0f,1.0f,1.0f };
		transform.rotate = { 0.0f,0.0f,0.0f };
		transform.translate = { count * 2.5f,0.0f,5.0f };

		triangle->SetTransform(transform);
		triangle->SetColor({ 1.0f,1.0f,1.0f,1.0f });

		triangles_.push_back(triangle);
		trianglesTransformData_.push_back(transform);
	}
}

//=================================================================================================//
// 更新処理

void GameScene::Update(Input* input) {

	input;

	//=============================================================================================//
	// Camera更新

	camera_->SetTransform(cameraTransform_);
	camera_->Update();

	//=============================================================================================//
	// 三角形更新
	for (int count = 0; count < triangleCounts_; ++count) {
		triangles_[count]->SetTransform(trianglesTransformData_[count]);

		triangles_[count]->Update(camera_->GetViewProjectionMatrix());
	}

#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::Begin("Triangle");

	ImGui::DragFloat3("CameraTranslate", &cameraTransform_.translate.x, 0.1f, -100.f, 100.f);
	ImGui::DragInt("TriangleCounts", &triangleCounts_, 1.f, 0, kTriangleCounts_);

	ImGui::End();

#endif
}

//=================================================================================================//
// 描画処理

void GameScene::Draw(ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	//=============================================================================================//
	// 三角形描画
	for (int count = 0; count < triangleCounts_; ++count) {
		triangles_[count]->Draw(commandList, graphicsSystem);
	}
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize() {

	//=============================================================================================//
	// 三角形解放
	for (int count = 0; count < static_cast<int>(triangles_.size()); ++count) {
		if (triangles_[count] != nullptr) {

			triangles_[count]->Finalize();

			delete triangles_[count];

			triangles_[count] = nullptr;
		}
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

void GameScene::SetResourceManager(ResourceManager* resourceManager) {

	resourceManager_ = resourceManager;
}

//=================================================================================================//
// Device設定

void GameScene::SetDevice(ID3D12Device* device) {

	device_ = device;
}

//=================================================================================================//
// 画面サイズ設定

void GameScene::SetWindowSize(int32_t width, int32_t height) {

	clientWidth_ = width;

	clientHeight_ = height;
}

//=================================================================================================//
// 終了判定

bool GameScene::IsFinished() {

	return isFinished_;
}

//=================================================================================================//
// シーン共通情報

void GameScene::SetContext(const SceneContext& sceneContext) {

	device_ = sceneContext.device;

	resourceManager_ = sceneContext.resourceManager;

	clientWidth_ = sceneContext.clientWidth;

	clientHeight_ = sceneContext.clientHeight;
}