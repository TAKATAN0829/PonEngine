#include "GameScene.h"
#include "TitleScene.h"
#include "serialization/SceneFile.h"
#include "serialization/LegacyLightMigration.h"
#include "serialization/ParticleEffectFile.h"
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

	initialAssetPath_.clear();

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

	// 共通設定は単一Emitterで動かすため、Effectファイルから対応可能な文書だけを読み込む。
	if (std::filesystem::exists(ParticleEffectFile::kDefaultPath, presetError)) {
		std::string presetMessage;

		if (!ParticleEffectFile::LoadSingleEmitter(*GetParticleSettings(), ParticleEffectFile::kDefaultPath, presetMessage)) {
			LogSystem::Log("Particle preset: " + presetMessage + "\n");
		}
	}

	// Releaseでも保存した編集用データを読み込み、制作時の配置をゲームへ引き継ぐ。
	std::error_code fileError;

	const bool hasSavedScene = std::filesystem::exists(SceneFile::kDefaultPath, fileError);

	bool wasSceneLoaded = false;

	if (fileError) {
		LogSystem::Log("Scene file check failed: " + fileError.message() + "\n");
	} else if (hasSavedScene) {
		std::string error;

		wasSceneLoaded = SceneFile::Load(*this, SceneFile::kDefaultPath, error,true);

		if (!wasSceneLoaded) {
			LogSystem::Log("Scene load failed: " + error + "\n");
		}
	}

	if (wasSceneLoaded) {
		initialAssetPath_ = SceneFile::kDefaultPath;
	}

	if (!wasSceneLoaded) {
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
	// 共通の補助画面を描き、Spriteの編集画面も表示する。
	WorldScene::DrawEditorTools(visibility);

	if (visibility.areWorldSettingsVisible) {
		DrawWorldSettings(&visibility.areWorldSettingsVisible);
	}
}

//=============================================================================================//
// 終了処理

void GameScene::Finalize() {
	// サンプルHUDを解放してから、共通のシーンを終了する。
	sprite_->Finalize();

	sprite_.reset();

	WorldScene::Finalize();
}

//=============================================================================================//
// Getter / 場面情報

const char* GameScene::GetName() const {
	// C++側の場面を識別する名前を返す。
	return "GameScene";
}

std::string GameScene::GetInitialAssetPath() const {
	// 起動時に読み込んだシーンファイルのパスを取得する。
	return initialAssetPath_;
}

//=============================================================================================//
// Setter / HUD表示サイズ

void GameScene::SetEditorViewport(uint32_t width, uint32_t height) {
	// SceneViewの寸法をカメラや描画基準へ反映する。
	WorldScene::SetEditorViewport(width, height);

	if (width == 0 || height == 0) {
		return;
	}

	spriteCommon_->SetViewportSize(static_cast<float>(width), static_cast<float>(height));
}

//=============================================================================================//
// 設定パネル描画

// Spriteの基本値を確認し、ゲーム停止中だけ設定を編集できるようにする。
void GameScene::DrawWorldSettings(bool* isOpen) {
#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	// 次のウィンドウサイズを設定。
	ImGui::SetNextWindowSize({390, 480}, ImGuiCond_FirstUseEver);
	// 「World Settings」ウィンドウを開始。
	if (!ImGui::Begin("World Settings", isOpen)) {
		// ウィンドウを終了。
		ImGui::End();

		return;
	}

	// 折り返しテキストを表示。
	ImGui::TextWrapped("ライトはHierarchyの右クリック → 新規作成 → Lightから作成し、Inspectorで編集します。");

	// Spriteの位置・回転・大きさを表示し、ゲーム再生中は誤編集を防ぐ。
	// 「Sprite」開閉見出しを表示。
	if (ImGui::CollapsingHeader("Sprite")) {
		// 操作無効化を開始。
		ImGui::BeginDisabled(IsPlaying());
		// 「Position」スライダーで2成分値を編集。
		ImGui::SliderFloat2("Position", &spritePosition_.x, 0.f, static_cast<float>(WinApp::kClientWidth));
		// 「Rotation」角度をスライダー編集。
		ImGui::SliderAngle("Rotation", &spriteRotation_);
		// 「Size」スライダーで2成分値を編集。
		ImGui::SliderFloat2("Size", &spriteSize_.x, 0.f, 512.f);
		// 操作無効化を終了。
		ImGui::EndDisabled();

	}

	// ウィンドウを終了。
	ImGui::End();

#else
	(void)isOpen;

#endif
}
