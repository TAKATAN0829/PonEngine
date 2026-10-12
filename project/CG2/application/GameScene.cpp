#include "GameScene.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=================================================================================================//
// 初期化処理

void GameScene::Initialize (const SceneContext& context) {

	//=============================================================================================//
	// Camera初期化

	camera_ = new Camera ();

	camera_->Initialize (WinApp::kClientWidth, WinApp::kClientHeight);

	cameraTransform_ = camera_->GetTransform ();

	cameraTransform_.translate = { 0.f,0.f,-30.f };

	debugCamera_ = new DebugCamera ();

	debugCamera_->Initialize (WinApp::kClientWidth, WinApp::kClientHeight);

	//=============================================================================================//
	// Particle初期化

	particle_ = new Particle ();

	particle_->Initialize (context.particleRenderer);

	//=============================================================================================//
	// Fence初期化

	fence_ = new Object3d ();

	fence_->Initialize (
		context.object3dRenderer,
		"resources/fence",
		"fence.obj");

	fence_->SetTexture ("resources/fence/fence.png");

	//=============================================================================================//
	// SpriteCommon取得

	spriteCommon_ = context.spriteCommon;

	//=============================================================================================//
	// Sprite初期化

	for (uint32_t i = 0; i < 5; ++i) {

		Sprite* sprite = new Sprite ();

		sprite->Initialize (spriteCommon_, "resources/uvChecker.png");

		sprite->SetPosition ({ 50.f + 200.f * static_cast<float>(i),50.f });

		sprites_.push_back (sprite);
	}
}

//=================================================================================================//
// 更新処理

void GameScene::Update (Input* input) {

	input;

	// Camera更新
	UpdateCamera (input);

	// Object更新
	UpdateObject ();
}

//=================================================================================================//
// 描画処理

void GameScene::Draw () {

	//=============================================================================================//
	// Particle描画

	particle_->Draw ();

	//=============================================================================================//
	// Fence描画

	fence_->Draw ();

	//=============================================================================================//
	// Sprite描画

	// Spriteの描画準備。Spriteの描画に共通のグラフィックスコマンドを積む
	spriteCommon_->DrawSettingCommon ();

	for (Sprite* sprite : sprites_) {

		sprite->Draw ();
	}
}

//=================================================================================================//
// 終了処理

void GameScene::Finalize () {

	//=============================================================================================//
	// Particle解放

	particle_->Finalize ();

	delete particle_;

	//=============================================================================================//
	// Fence解放

	fence_->Finalize ();

	delete fence_;

	//=============================================================================================//
	// Sprite解放

	for (Sprite* sprite : sprites_) {

		delete sprite;
	}

	sprites_.clear ();

	//=============================================================================================//
	// Camera解放

	delete camera_;
	delete debugCamera_;

}

//=================================================================================================//
// Camera更新

void GameScene::UpdateCamera (Input* input) {

	camera_->SetTransform (cameraTransform_);

	camera_->Update ();

	debugCamera_->Update (input);
}

//=================================================================================================//
// Object更新

void GameScene::UpdateObject () {

	//=============================================================================================//
	// Particle更新

	particle_->SetColor (color_);
	particle_->SetBlendMode (blendMode_);
	particle_->Update (debugCamera_->GetViewProjectionMatrix ());

	//=============================================================================================//
	// Fence更新

	fence_->SetTransform (fenceTransform_);
	fence_->SetBlendMode (blendMode_);
	fence_->Update (debugCamera_->GetViewProjectionMatrix ());

	//=============================================================================================//
	// Sprite更新

	for (Sprite* sprite : sprites_) {

		sprite->Update ();
	}

#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::Begin ("Settings");
	ImGui::ColorEdit4 ("color", &color_.x);

	static const char* kBlendModeNames[kCountOfBlendMode] = {
		"kBlendModeNone",
		"kBlendModeNormal",
		"kBlendModeAdd",
		"kBlendModeSubtract",
		"kBlendModeMultiply",
		"kBlendModeScreen",
		"kBlendModeLighten",
		"kBlendModeDarken",
		"kBlendModeInvert",
	};

	int blendMode = static_cast<int>(blendMode_);

	if (ImGui::Combo (
		"Blend",
		&blendMode,
		kBlendModeNames,
		kCountOfBlendMode)) {

		blendMode_ = static_cast<BlendMode>(blendMode);
	}

	ImGui::End ();

	ImGui::Begin ("Fence");
	ImGui::DragFloat3 (
		"FenceRotate",
		&fenceTransform_.rotate.x,
		0.01f);
	ImGui::DragFloat3 (
		"FenceTranslate",
		&fenceTransform_.translate.x,
		0.1f);
	ImGui::End ();

	//=============================================================================================//
	// Sprite

	ImGui::Begin ("Sprite");

	for (size_t i = 0; i < sprites_.size (); ++i) {

		Sprite* sprite = sprites_[i];

		// 同じラベルが並ぶので、Spriteごとに別IDにする
		ImGui::PushID (static_cast<int>(i));

		// Sprite番号ごとに折りたたみ表示
		if (ImGui::TreeNode ("Sprite", "Sprite %zu", i)) {

			// Position
			Vector2 position = sprite->GetPosition ();

			if (ImGui::DragFloat2 ("Position", &position.x, 1.f)) {

				sprite->SetPosition (position);
			}

			// Rotation
			float rotation = sprite->GetRotation ();

			if (ImGui::SliderAngle ("Rotation", &rotation)) {

				sprite->SetRotation (rotation);
			}

			// Size
			Vector2 size = sprite->GetSize ();

			if (ImGui::DragFloat2 ("Size", &size.x, 1.f)) {

				sprite->SetSize (size);
			}

			// AnchorPoint
			Vector2 anchorPoint = sprite->GetAnchorPoint ();

			if (ImGui::SliderFloat2 ("AnchorPoint", &anchorPoint.x, 0.f, 1.f)) {

				sprite->SetAnchorPoint (anchorPoint);
			}

			// FlipX
			bool isFlipX = sprite->GetIsFlipX ();

			if (ImGui::Checkbox ("FlipX", &isFlipX)) {

				sprite->SetIsFlipX (isFlipX);
			}

			// FlipY
			bool isFlipY = sprite->GetIsFlipY ();

			if (ImGui::Checkbox ("FlipY", &isFlipY)) {

				sprite->SetIsFlipY (isFlipY);
			}

			// TextureLeftTop
			Vector2 textureLeftTop = sprite->GetTextureLeftTop ();

			if (ImGui::DragFloat2 ("TextureLeftTop", &textureLeftTop.x, 1.f)) {

				sprite->SetTextureLeftTop (textureLeftTop);
			}

			// TextureSize
			Vector2 textureSize = sprite->GetTextureSize ();

			if (ImGui::DragFloat2 ("TextureSize", &textureSize.x, 1.f)) {

				sprite->SetTextureSize (textureSize);
			}

			// Color
			Vector4 color = sprite->GetColor ();

			if (ImGui::ColorEdit4 ("Color", &color.x)) {

				sprite->SetColor (color);
			}

			ImGui::TreePop ();
		}

		ImGui::PopID ();
	}

	ImGui::End ();

#endif
}