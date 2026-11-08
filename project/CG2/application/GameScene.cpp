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
		// 読込関数から失敗理由を受け取る文字列を用意する。
		std::string presetMessage;

		// ファイルを読み、共通Particleが使える単一Emitterの設定へ変換する。
		if (!ParticleEffectFile::LoadSingleEmitter(*GetParticleSettings(), ParticleEffectFile::kDefaultPath, presetMessage)) {
			// プリセットを使えない理由をログに残し、シーンの初期化を続ける。
			LogSystem::Log("Particle preset: " + presetMessage + "\n");
		}
	}

	// Releaseでも保存した編集用データを読み込み、制作時の配置をゲームへ引き継ぐ。
	std::error_code fileError;

	// 保存ファイルがない場合と、存在確認そのものに失敗した場合を分ける。
	const bool hasSavedScene = std::filesystem::exists(SceneFile::kDefaultPath, fileError);

	// ファイルがないときも既定配置を使えるよう、読込成功まではfalseのままにする。
	bool wasSceneLoaded = false;

	// 存在確認に失敗した場合は、ファイルを開く前のエラーとして記録する。
	if (fileError) {
		LogSystem::Log("Scene file check failed: " + fileError.message() + "\n");
	} else if (hasSavedScene) {
		std::string error;

		// 現在のGameSceneへ読み込み、末尾のtrueで旧形式のライト補完を許可する。
		wasSceneLoaded = SceneFile::Load(*this, SceneFile::kDefaultPath, error,true);

		// 存在していても、壊れたデータや未対応の形式なら理由をログへ出す。
		if (!wasSceneLoaded) {
			LogSystem::Log("Scene load failed: " + error + "\n");
		}
	}

	// 読込成功時だけ、このファイルをEditorの最初の保存先として引き継ぐ。
	if (wasSceneLoaded) {
		initialAssetPath_ = SceneFile::kDefaultPath;
	}

	// 保存データを使えなければ、先に用意した既定の配置へライトを追加する。
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

void GameScene::DrawWorldSettings(bool* isOpen) {
#ifdef USE_IMGUI

	//=============================================================================================//
	// ImGui

	ImGui::SetNextWindowSize({390, 480}, ImGuiCond_FirstUseEver);

	if (!ImGui::Begin("World Settings", isOpen)) {
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
	(void)isOpen;

#endif
}
