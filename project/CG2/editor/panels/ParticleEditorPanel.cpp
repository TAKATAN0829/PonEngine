#include "ParticleEditorPanel.h"
#include "ParticlePresetFile.h"
#include "LogSystem.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cfloat>

namespace {
	// 狭いInspector幅でもラベルと数値が重ならないよう、ラベルの下へ値を並べる。
	void EditParticleVector(const char* label, Vector3& value, float minimum, float maximum) {
		ImGui::PushID(label);
		ImGui::TextUnformatted(label);
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::DragFloat3("##value", &value.x, 0.05f, minimum, maximum, "%.2f", ImGuiSliderFlags_AlwaysClamp);
		ImGui::PopID();
	}

	// 発生条件と配置は同じカテゴリで編集する。
	void DrawEmitterSettings(ParticleSettings& settings, TransformData* transform) {
		if (ImGui::CollapsingHeader("Emitter / 発生", ImGuiTreeNodeFlags_DefaultOpen)) {
			if (transform != nullptr) {
				EditParticleVector("発生位置", transform->translate, -1000, 1000);
			}
			else {
				EditParticleVector("発生位置", settings.emitterPosition, -1000, 1000);
			}
			EditParticleVector("拡散範囲", settings.spread, 0, 100);
			int spawnCount = static_cast<int>(settings.spawnCount);
			ImGui::TextUnformatted("一度の発生数");
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::SliderInt("##SpawnCount", &spawnCount, 1, 100);
			settings.spawnCount = static_cast<uint32_t>(spawnCount);
			ImGui::TextUnformatted("発生間隔（秒）");
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloat("##Interval", &settings.interval, 0.01f, 0.01f, 60, "%.2f", ImGuiSliderFlags_AlwaysClamp);
			ImGui::TextUnformatted("寿命（秒）");
			ImGui::SetNextItemWidth(-FLT_MIN);
			ImGui::DragFloatRange2("##Lifetime", &settings.lifetimeMinimum, &settings.lifetimeMaximum,
				0.05f, 0.01f, 60, "min %.2f", "max %.2f", ImGuiSliderFlags_AlwaysClamp);
		}
	}

	// 速度と加速度の編集に使う値は、この関数内で完結させる。
	void DrawMotionSettings(ParticleSettings& settings) {
		if (ImGui::CollapsingHeader("Motion / 動き", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("最小速度", settings.velocityMinimum, -100, 100);
			EditParticleVector("最大速度", settings.velocityMaximum, -100, 100);
			ImGui::Checkbox("加速度を使う", &settings.accelerationEnabled);
			EditParticleVector("加速度", settings.acceleration, -100, 100);
		}
	}

	// 画像・合成方法・色を設定する。Combo用の添字はここだけで使用する。
	void DrawAppearanceSettings(ParticleSettings& settings) {
		if (ImGui::CollapsingHeader("Appearance / 見た目", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("粒子の大きさ", settings.particleScale, 0.01f, 100);
			ImGui::ColorEdit4("色", &settings.color.x);
			ImGui::Checkbox("ランダムな色", &settings.randomColor);
			ImGui::Checkbox("カメラへ向ける", &settings.billboard);
			const char* textures[] = {"resources/circle.png", "resources/uvChecker.png", "resources/white1x1.png"};
			int textureIndex = 0;
			for (int candidateIndex = 0; candidateIndex < 3; ++candidateIndex) {
				if (settings.texture == textures[candidateIndex]) {
					textureIndex = candidateIndex;
				}
			}
			if (ImGui::Combo("画像", &textureIndex, textures, 3)) {
				settings.texture = textures[textureIndex];
			}
			const char* blends[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Lighten", "Darken", "Invert"};
			int blendIndex = static_cast<int>(settings.blendMode);
			if (ImGui::Combo("合成", &blendIndex, blends, kCountOfBlendMode)) {
				settings.blendMode = static_cast<BlendMode>(blendIndex);
			}
		}
	}
}
#endif

// 実行中の粒子数ではなく、保存対象の設定だけで未保存状態を判定する。
bool ParticleEditorPanel::IsDirty(IEditableScene& scene) const {
	const ParticleSettings* settings = scene.GetParticleSettings();
	return scene_ == &scene && settings != nullptr && !EqualParticleSettings(*settings, savedSettings_);
}

// 保存成功時だけ比較基準を更新し、失敗した設定を保存済み扱いにしない。
bool ParticleEditorPanel::Save(IEditableScene& scene) {
	const ParticleSettings* settings = scene.GetParticleSettings();
	if (settings == nullptr) {
		return true;
	}
	if (!ParticlePresetFile::Save(*settings, ParticlePresetFile::kDefaultPath, status_)) {
		LogSystem::Log(status_ + "\n");
		return false;
	}
	savedSettings_ = *settings;
	status_ = "プリセットを保存しました。";
	LogSystem::Log("Particle preset saved.\n");
	return true;
}

// 選択がParticleの場合はそのComponentを編集し、それ以外では共通プリセットを編集する。
ParticleSettings* ParticleEditorPanel::GetEditingSettings(IEditableScene& scene) const {
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == targetId_) {
			if (object.type == SceneObjectType::Particle) {
				return object.particleSettings;
			}
		}
	}
	return scene.GetParticleSettings();
}

// 個別Emitterのプリセット書出しでは、共通設定の保存基準を更新しない。
bool ParticleEditorPanel::SaveEditingPreset(IEditableScene& scene) {
	if (targetId_ == kInvalidSceneObjectId) {
		return Save(scene);
	}
	const ParticleSettings* settings = GetEditingSettings(scene);
	if (settings == nullptr) {
		return false;
	}
	const bool saved = ParticlePresetFile::Save(*settings, ParticlePresetFile::kDefaultPath, status_);
	if (saved) {
		status_ = "選択Emitterをプリセットへ書き出しました。配置はシーン保存で保存できます。";
	}
	return saved;
}

// 設定変更の破棄が確認できた操作だけを実行する。
void ParticleEditorPanel::ApplyPendingAction(IEditableScene& scene) {
	ParticleSettings* settings = GetEditingSettings(scene);
	if (settings == nullptr) {
		return;
	}
	if (pendingAction_ == PresetAction::Load) {
		if (ParticlePresetFile::Load(*settings, ParticlePresetFile::kDefaultPath, status_)) {
			if (targetId_ == kInvalidSceneObjectId) {
				savedSettings_ = *settings;
			}
			status_ = "プリセットを読み込みました。";
			scene.ResetParticlePreview();
		}
	} else if (pendingAction_ == PresetAction::Defaults) {
		*settings = ParticleSettings{};
		scene.ResetParticlePreview();
		status_ = "既定値に戻しました。保存すると次回も使用できます。";
	}
	pendingAction_ = PresetAction::None;
}

// 注視要求はSceneパネル側が一度だけ消費する。
bool ParticleEditorPanel::ConsumeFocusRequest() {
	const bool requested = focusRequested_;
	focusRequested_ = false;
	return requested;
}

//=============================================================================================//
// 初期化処理

void ParticleEditorPanel::Initialize(IEditableScene& scene) {
	scene_ = &scene;
	if (scene.GetParticleSettings() != nullptr) {
		savedSettings_ = *scene.GetParticleSettings();
	}
	preview_ = {};
	targetId_ = kInvalidSceneObjectId;
	focusPosition_ = {};
	status_.clear();
	pendingAction_ = PresetAction::None;
	openPrompt_ = false;
	focusRequested_ = false;
}

//=============================================================================================//
// 更新処理

void ParticleEditorPanel::Update(IEditableScene& scene, bool gamePlaying, SceneObjectId selectedId) {
	if (scene_ != &scene) {
		Initialize(scene);
	}
	targetId_ = kInvalidSceneObjectId;
	TransformData* emitterTransform = nullptr;
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == selectedId) {
			if (object.type == SceneObjectType::Particle) {
				targetId_ = object.id;
				emitterTransform = object.transform;
				break;
			}
		}
	}
	// 再生中の選択変更でゲーム内の粒子を消さない。
	if (!gamePlaying) {
		if (preview_.targetObject != targetId_) {
			scene.ResetParticlePreview();
		}
	}
	preview_.targetObject = targetId_;
	const ParticleSettings* settings = GetEditingSettings(scene);
	if (settings != nullptr) {
		focusPosition_ = settings->emitterPosition;
	}
	if (emitterTransform != nullptr) {
		focusPosition_ = scene.GetObjectWorldPosition(targetId_);
	}
}

// 選択中のEmitterだけがTransformを発生位置として使う。
TransformData* ParticleEditorPanel::GetEditingTransform(IEditableScene& scene) const {
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		if (object.id == targetId_) {
			if (object.type == SceneObjectType::Particle) {
				return object.transform;
			}
		}
	}
	return nullptr;
}

//=============================================================================================//
// 描画処理

void ParticleEditorPanel::Draw(IEditableScene& scene, bool gamePlaying, SceneObjectId selectedId, bool* open) {
	Update(scene, gamePlaying, selectedId);
#ifdef USE_IMGUI
	ParticleSettings* settings = GetEditingSettings(scene);
	TransformData* emitterTransform = GetEditingTransform(scene);
	ImGui::SetNextWindowSize({420, 560}, ImGuiCond_FirstUseEver);
	if (ImGui::Begin("Particle Editor", open)) {
		if (settings == nullptr) {
			ImGui::TextUnformatted("このシーンにはParticleがありません。");
		} else {
			ImGui::TextUnformatted("PARTICLE / 発生設定");
			if (targetId_ != kInvalidSceneObjectId) {
				ImGui::TextWrapped("選択中のParticleを編集しています。Ctrl+Sでシーンに保存します。");
				ImGui::TextWrapped("発生位置はTransformと共通のローカル値です。");
			}
			ImGui::TextWrapped("%s", ParticlePresetFile::kDefaultPath);
			if (targetId_ == kInvalidSceneObjectId) {
				if (IsDirty(scene)) {
					ImGui::TextUnformatted("* 未保存の変更");
				}
			}
			ImGui::TextWrapped("Scene内でプレビューします。ゲーム停止中にも調整できます。");
			ImGui::BeginDisabled(gamePlaying);
			if (!preview_.enabled) {
				if (ImGui::Button("プレビュー開始")) {
					preview_.enabled = true;
					preview_.paused = false;
					preview_.automaticEmission = true;
					scene.ResetParticlePreview();
					focusRequested_ = true;
				}
			} else {
				if (ImGui::Button("プレビュー停止")) {
					preview_.enabled = false;
					scene.ResetParticlePreview();
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("一時停止 / 再開")) {
				preview_.paused = !preview_.paused;
			}
			if (ImGui::Button("一度だけ発生")) {
				preview_.enabled = true;
				preview_.paused = false;
				preview_.automaticEmission = false;
				scene.BurstParticles();
				focusRequested_ = true;
			}
			ImGui::SameLine();
			if (ImGui::Button("リセット")) {
				scene.ResetParticlePreview();
			}
			ImGui::BeginDisabled(!preview_.enabled || !preview_.paused);
			if (ImGui::Button("1フレーム進める")) {
				preview_.step = true;
			}
			ImGui::EndDisabled();
			ImGui::Checkbox("Particleだけを表示", &preview_.isolate);
			if (ImGui::Button("発生位置へカメラを移動")) {
				focusRequested_ = true;
			}
			ImGui::Text("粒子数: %zu / 100", scene.GetParticleCount());
			ImGui::Separator();
			if (ImGui::Button("プリセット書出し")) {
				SaveEditingPreset(scene);
			}
			ImGui::SameLine();
			if (ImGui::Button("読込")) {
				pendingAction_ = PresetAction::Load;
			}
			ImGui::SameLine();
			if (ImGui::Button("既定値")) {
				pendingAction_ = PresetAction::Defaults;
			}
			if (!openPrompt_) {
				if (pendingAction_ != PresetAction::None) {
					if (targetId_ == kInvalidSceneObjectId) {
						if (IsDirty(scene)) {
							ImGui::OpenPopup("Particleの未保存変更");
							openPrompt_ = true;
						}
						else {
							ApplyPendingAction(scene);
						}
					} else {
						ApplyPendingAction(scene);
					}
				}
			}
			if (ImGui::BeginPopupModal("Particleの未保存変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
				ImGui::TextUnformatted("変更を保存してから設定を置き換えますか？");
				if (ImGui::Button("保存して続行")) {
					if (SaveEditingPreset(scene)) {
						ApplyPendingAction(scene);
						openPrompt_ = false;
						ImGui::CloseCurrentPopup();
					}
				}
				if (ImGui::Button("破棄して続行")) {
					ApplyPendingAction(scene);
					openPrompt_ = false;
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::Button("キャンセル")) {
					pendingAction_ = PresetAction::None;
					openPrompt_ = false;
					ImGui::CloseCurrentPopup();
				}
				ImGui::EndPopup();
			}
			DrawEmitterSettings(*settings, emitterTransform);
			DrawMotionSettings(*settings);
			DrawAppearanceSettings(*settings);
			ImGui::EndDisabled();
			if (!ValidateParticleSettings(*settings)) {
				ImGui::TextWrapped("最小値と最大値などを確認してください。不正な設定は実行・保存しません。");
			}
			if (!status_.empty()) {
				ImGui::TextWrapped("%s", status_.c_str());
			}
		}
	}
	ImGui::End();
#else
	(void)gamePlaying;
	(void)open;
#endif
	scene.SetParticlePreview(preview_);
	preview_.step = false;
}

// 閉じた補助画面のプレビューが、見えないまま走り続けないようにする。
void ParticleEditorPanel::StopPreview(IEditableScene& scene, bool gamePlaying) {
	if (preview_.enabled) {
		preview_.enabled = false;
		preview_.step = false;
		// 再生中のゲームにある粒子は、補助画面を閉じても消さない。
		if (!gamePlaying) {
			scene.ResetParticlePreview();
		}
	}
	scene.SetParticlePreview(preview_);
}
