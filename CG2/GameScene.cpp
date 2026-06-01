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

	cameraTransform_.translate = { 0.f,0.f,-30.f };

	debugCamera_ = new DebugCamera();

	debugCamera_->GetTransform();

	debugCamera_->Initialize(clientWidth_, clientHeight_);
	//=============================================================================================//
	// 三角形初期化

	triangles_.resize(triangleCountsX_ * triangleCountsY_);
	trianglesTransformData_.resize(triangleCountsX_ * triangleCountsY_);

	invTriangles_.resize(triangleCountsX_ * triangleCountsY_);
	invTrianglesTransformData_.resize(triangleCountsX_ * triangleCountsY_);

	// 三角形1個あたりのTransformData 
	triangleTransform_ = {
		{1.f,1.f,1.f},
		{0.f,0.f,0.f},
	};

	invTriangleTransform_ = {
		{1.f,1.f,1.f},
		{0.f,0.f,-3.141529f},
	};

	for (int countY = 0; countY < triangleCountsY_; ++countY) {
		for (int countX = 0; countX < triangleCountsX_; ++countX) {

			int index = countY * triangleCountsX_ + countX;

			triangles_[index] = new Object3d();
			triangles_[index]->Initialize(MeshType::kTriangle);

			trianglesTransformData_[index].scale = { triangleTransform_.scale };
			trianglesTransformData_[index].rotate = { triangleTransform_.rotate };

			// 画面中央を基準に並べる
			trianglesTransformData_[index].translate = {
				(countX - triangleCountsX_ * 0.5f) * 2.0f,
				(countY - triangleCountsY_ * 0.5f) * 2.0f,
				0.0f
			};

			triangles_[index]->SetTransform(trianglesTransformData_[index]);
			triangles_[index]->SetColor({ 1.0f,1.0f,1.0f,1.0f });

			invTriangles_[index] = new Object3d();
			invTriangles_[index]->Initialize(MeshType::kTriangle);

			invTrianglesTransformData_[index].scale = { invTriangleTransform_.scale };
			invTrianglesTransformData_[index].rotate = { invTriangleTransform_.rotate };

			invTrianglesTransformData_[index].translate = {
				(countX - triangleCountsX_ * 0.5f) * 2.0f + 1.0f,
				(countY - triangleCountsY_ * 0.5f) * 2.0f + 2.0f,
				0.0f
			};

			invTriangles_[index]->SetTransform(invTrianglesTransformData_[index]);
			invTriangles_[index]->SetColor({ 0.0f,0.0f,0.0f,1.0f });

		}
	}
}

//=================================================================================================//
// 更新処理

void GameScene::Update(Input* input) {

	input;

#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::Begin("Triangle");

	ImGui::DragFloat3("CameraTranslate", &cameraTransform_.translate.x, 0.1f, -30.0f, 30.0f);

	ImGui::DragFloat3("TriangleScale", &triangleTransform_.scale.x, 0.1f, 0.f, 10.f);
	ImGui::DragFloat3("TriangleRotate", &triangleTransform_.rotate.x, 0.1f, 0.f, 10.f);

	ImGui::End();

#endif

	//=============================================================================================//
	// Camera更新

	camera_->SetTransform(cameraTransform_);

	camera_->Update();

	//=============================================================================================//
	// 三角形更新

	for (int countY = 0; countY < triangleCountsY_; ++countY) {
		for (int countX = 0; countX < triangleCountsX_; ++countX) {

			int index = countY * triangleCountsX_ + countX;
			trianglesTransformData_[index].scale = triangleTransform_.scale;
			trianglesTransformData_[index].rotate = triangleTransform_.rotate;

			triangles_[index]->SetTransform(trianglesTransformData_[index]);
			triangles_[index]->Update(camera_->GetViewProjectionMatrix());

			invTriangles_[index]->SetTransform(invTrianglesTransformData_[index]);
			invTriangles_[index]->Update(camera_->GetViewProjectionMatrix());

		}
	}

	debugCamera_->Update(input);
	
}

//=================================================================================================//
// 描画処理

void GameScene::Draw() {

	//=============================================================================================//
	// 三角形描画

	for (int countY = 0; countY < triangleCountsY_; ++countY) {
		for (int countX = 0; countX < triangleCountsX_; ++countX) {

			int index = countY * triangleCountsX_ + countX;

			triangles_[index]->Draw();
			invTriangles_[index]->Draw();
		}
	}
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize() {

	//=============================================================================================//
	// 三角形解放

	for (Object3d* triangle : triangles_) {

		if (triangle != nullptr) {

			triangle->Finalize();

			delete triangle;
		}
	}

	for (Object3d* invTriangle : invTriangles_) {

		if (invTriangle != nullptr) {

			invTriangle->Finalize();

			delete invTriangle;
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

	clientWidth_ = sceneContext.clientWidth;

	clientHeight_ = sceneContext.clientHeight;
}