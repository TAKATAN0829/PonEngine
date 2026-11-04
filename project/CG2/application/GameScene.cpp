#include "GameScene.h"
#include "TitleScene.h"
#include "serialization/SceneFile.h"
#include "serialization/LegacyLightMigration.h"
#include "serialization/ParticlePresetFile.h"
#include "ModelManager.h"
#include "LogSystem.h"
#include "WinApp.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

//=============================================================================================//
// 初期化処理

void GameScene::Initialize(const SceneContext& context) {
	WorldScene::Initialize(context);

	SetCommonParticleEmission(true);

	//=============================================================================================//
	// モデル初期化

	ModelManager::GetInstance()->LoadModel("plane.gltf");

	ModelManager::GetInstance()->LoadModel("axis.obj");

	WorldObjectData mainModel;

	mainModel.id = 1;

	mainModel.name = "Plane #1";

	mainModel.transform.scale = {5, 5, 5};

	WorldObjectData axis;

	axis.id = 2;

	axis.name = "Axis #2";

	axis.model = "axis.obj";

	axis.transform.translate = {3, 0, 0};

	std::string sceneError;

	ReplaceObjects({mainModel, axis}, sceneError);

	std::error_code presetError;

	if (std::filesystem::exists(ParticlePresetFile::kDefaultPath, presetError)) {
		std::string presetMessage;

		if (!ParticlePresetFile::Load(*GetParticleSettings(), ParticlePresetFile::kDefaultPath, presetMessage)) {
			LogSystem::Log("Particle preset: " + presetMessage + "\n");
		}
	}

	// Releaseでも保存した編集用データを読み込み、制作時の配置をゲームへ引き継ぐ。
	std::error_code fileError;

	const bool hasSavedScene = std::filesystem::exists(SceneFile::kDefaultPath, fileError);

	bool loadedScene = false;

	if (fileError) {
		LogSystem::Log("Scene file check failed: " + fileError.message() + "\n");
	} else if (hasSavedScene) {
		std::string error;

		loadedScene = SceneFile::Load(*this, SceneFile::kDefaultPath, error,true);

		if (!loadedScene) {
			LogSystem::Log("Scene load failed: " + error + "\n");
		}
	}

	if (!loadedScene) {
		std::vector<WorldObjectData> initial = CaptureObjects();

		if (!LegacyLightMigration::AddLegacyDefaults(initial,sceneError)) {
			LogSystem::Log("Default lights: "+sceneError+"\n");
		} else {
			if (!ReplaceObjects(std::move(initial),sceneError)) {
				LogSystem::Log("Default lights: "+sceneError+"\n");
			}
		}
	}

	//=============================================================================================//
	// Sprite初期化

	spriteCommon_ = context.spriteCommon;

	sprite_ = std::make_unique<Sprite>();

	sprite_->Initialize(spriteCommon_, "resources/uvChecker.png");

	// Releaseでも最初のKinematic移動より前に初期位置を取得する。
	InitializeSimulation();
}

//=============================================================================================//
// 更新処理

void GameScene::Update(Input* input) {
	// 入力はゲーム更新中だけ扱い、編集・Pause中は場面を変えない。
	if (input != nullptr) {
		if (CanHandleGameInput()) {
			if (input->IsTriggerKey(DIK_ESCAPE)) {
				RequestSceneChange(std::make_unique<TitleScene>());
			}
		}
	}

	WorldScene::Update(input);

}

//=============================================================================================//
// 描画処理

void GameScene::Draw() {
	WorldScene::Draw();

	if (ShouldDrawGameOverlay()) {
		// 編集中もHUDの設定変更を反映する。3D描画の共通処理へは持ち込まない。
		sprite_->SetPosition(spritePosition_);

		sprite_->SetRotation(spriteRotation_);

		sprite_->SetSize(spriteSize_);

		sprite_->Update();

		spriteCommon_->DrawSettingCommon();

		sprite_->Draw();
	}
}

void GameScene::DrawEditorTools(EditorToolVisibility& visibility) {
	WorldScene::DrawEditorTools(visibility);

	if (visibility.worldSettings) {
		DrawWorldSettings(&visibility.worldSettings);
	}
}

//=============================================================================================//
// 終了処理

void GameScene::Finalize() {
	sprite_->Finalize();

	sprite_.reset();

	WorldScene::Finalize();
}

//=============================================================================================//
// Setter / HUD表示サイズ

void GameScene::SetEditorViewport(uint32_t width, uint32_t height) {
	WorldScene::SetEditorViewport(width, height);

	if (width == 0 || height == 0) {
		return;
	}

	spriteCommon_->SetViewportSize(static_cast<float>(width), static_cast<float>(height));
}

//=============================================================================================//
// 設定パネル描画

void GameScene::DrawWorldSettings(bool* open) {
#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::SetNextWindowSize({390, 480}, ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("World Settings", open)) {
		ImGui::End();

		return;
	}

	ImGui::TextWrapped("ライトはHierarchyの右クリック → 新規作成 → Lightから作成し、Inspectorで編集します。");

	if (ImGui::CollapsingHeader("Sprite")) {

		ImGui::BeginDisabled(IsPlaying());

		ImGui::SliderFloat2("Position", &spritePosition_.x, 0.f, static_cast<float>(WinApp::kClientWidth));

		ImGui::SliderAngle("Rotation", &spriteRotation_);

		ImGui::SliderFloat2("Size", &spriteSize_.x, 0.f, 512.f);

		ImGui::EndDisabled();

	}

	ImGui::End();

#else
	(void)open;

#endif
}
