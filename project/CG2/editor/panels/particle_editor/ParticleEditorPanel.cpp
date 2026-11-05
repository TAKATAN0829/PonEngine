#include "ParticleEditorPanel.h"
#include "AssetPathChange.h"
#include "serialization/ParticlePresetFile.h"
#include "LogSystem.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <cfloat>

//=============================================================================================//
// 実装内補助定義

namespace {
	// 狭いInspector幅でもラベルと数値が重ならないよう、ラベルの下へ値を並べる。
	void EditParticleVector(const char* label, Vector3& value, float minimum, float maximum) {
		// Particle用のベクトル設定を編集し、変更の有無を返す。
		ImGui::PushID(label);

		ImGui::TextUnformatted(label);

		ImGui::SetNextItemWidth(-FLT_MIN);

		ImGui::DragFloat3("##value", &value.x, 0.05f, minimum, maximum, "%.2f", ImGuiSliderFlags_AlwaysClamp);

		ImGui::PopID();
	}

	// 発生条件と配置は同じカテゴリで編集する。
	void DrawEmitterSettings(ParticleSettings& settings, TransformData* transform) {
		// 発生数・間隔・位置など、Emitterの発生条件を編集する。
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
		// 粒子の速度と加速度の最小・最大値を編集する。
		if (ImGui::CollapsingHeader("Motion / 動き", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("最小速度", settings.velocityMinimum, -100, 100);

			EditParticleVector("最大速度", settings.velocityMaximum, -100, 100);

			ImGui::Checkbox("加速度を使う", &settings.isAccelerationEnabled);

			EditParticleVector("加速度", settings.acceleration, -100, 100);
		}
	}

	// 画像・合成方法・色を設定する。Combo用の添字はここだけで使用する。
	void DrawAppearanceSettings(ParticleSettings& settings) {
		if (ImGui::CollapsingHeader("Appearance / 見た目", ImGuiTreeNodeFlags_DefaultOpen)) {
			EditParticleVector("粒子の大きさ", settings.particleScale, 0.01f, 100);

			ImGui::ColorEdit4("色", &settings.color.x);

			ImGui::Checkbox("ランダムな色", &settings.isRandomColorEnabled);

			ImGui::Checkbox("カメラへ向ける", &settings.isBillboardEnabled);

			const char* textures[] = {"resources/circle.png", "resources/uvChecker.png", "resources/white1x1.png"};

			// 移動・改名済みの画像も、実際の参照先をそのまま表示する。
			if (ImGui::BeginCombo("画像", settings.texture.c_str())) {
				for (const char* texture : textures) {
					if (ImGui::Selectable(texture, settings.texture == texture)) {
						settings.texture = texture;
					}
				}

				ImGui::EndCombo();
			}

			ImGui::TextDisabled("画像の透明度を使用 / 不透明画像は明るさをマスクに使用");

			const char* blends[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Lighten", "Darken", "Invert"};

			int blendIndex = static_cast<int>(settings.blendMode);

			if (ImGui::Combo("合成", &blendIndex, blends, kCountOfBlendMode)) {
				settings.blendMode = static_cast<BlendMode>(blendIndex);
			}

			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("発光はAdd、煙や半透明の粒はNormal。Noneは半透明合成を行いません。");
			}

			ImGui::Checkbox("Soft Particles",&settings.areSoftParticlesEnabled);

			ImGui::BeginDisabled(!settings.areSoftParticlesEnabled);

			ImGui::DragFloat("交差をぼかす距離",&settings.softDistance,0.01f,0.001f,100,"%.3f",ImGuiSliderFlags_AlwaysClamp);

			ImGui::EndDisabled();

			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
				ImGui::SetTooltip("モデルとの交差をワールド距離でぼかします。Particleだけの表示では深度がないため効果は出ません。");
			}
		}
	}

	// 開始色はAppearance、終了色とサイズ倍率は寿命で補間する。
	void DrawLifetimeSettings(ParticleSettings& settings) {
		// 粒子の寿命に応じた変化と消え方を編集する。
		if (!ImGui::CollapsingHeader("Lifetime / 寿命による変化",ImGuiTreeNodeFlags_DefaultOpen)) {
			return;
		}

		ImGui::Checkbox("サイズ・色を変化させる",&settings.isLifetimeAppearanceEnabled);

		ImGui::BeginDisabled(!settings.isLifetimeAppearanceEnabled);

		ImGui::DragFloat("開始サイズ倍率",&settings.startSize,0.01f,0.001f,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);

		ImGui::DragFloat("終了サイズ倍率",&settings.endSize,0.01f,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);

		ImGui::ColorEdit4("終了色",&settings.endColor.x);

		ImGui::SliderFloat("フェードイン（寿命比）",&settings.fadeIn,0,1,"%.2f");

		ImGui::EndDisabled();
	}
}

#endif

//=============================================================================================//
// 初期化処理

void ParticleEditorPanel::Initialize(IEditableWorld& scene) {
	// 共通プリセットを保存基準とし、プレビューの状態を初期化する。
	scene_ = &scene;

	if (scene.GetParticleSettings() != nullptr) {
		savedSettings_ = *scene.GetParticleSettings();
	}

	preview_ = {};

	targetId_ = kInvalidWorldObjectId;

	focusPosition_ = {};

	status_.clear();

	pendingAction_ = PresetAction::None;

	shouldOpenPrompt_ = false;

	isFocusRequested_ = false;
}

void ParticleEditorPanel::ResetScenePreview(IEditableWorld& scene) {
	// 保存済み設定を保持したまま、対象選択とプレビュー要求を初期化する。
	scene_ = &scene;

	preview_ = {};

	targetId_ = kInvalidWorldObjectId;

	focusPosition_ = {};

	pendingAction_ = PresetAction::None;

	shouldOpenPrompt_ = false;

	isFocusRequested_ = false;

	scene.SetParticlePreview(preview_);

	scene.ResetParticlePreview();
}

//=============================================================================================//
// 更新処理

void ParticleEditorPanel::Update(IEditableWorld& scene, bool isGamePlaying, WorldObjectId selectedId) {
	if (scene_ != &scene) {
		Initialize(scene);
	}

	targetId_ = kInvalidWorldObjectId;

	TransformData* emitterTransform = nullptr;

	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.id == selectedId) {
			if (object.type == WorldObjectType::Particle) {
				targetId_ = object.id;

				emitterTransform = object.transform;

				break;
			}
		}
	}

	// 再生中の選択変更でゲーム内の粒子を消さない。
	if (!isGamePlaying) {
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

//=============================================================================================//
// 描画処理

void ParticleEditorPanel::Draw(IEditableWorld& scene, bool isGamePlaying, WorldObjectId selectedId, bool* isOpen) {
	// 編集対象と再生状態を確認し、発生・動き・見た目の編集欄を表示する。
	Update(scene, isGamePlaying, selectedId);

#ifdef USE_IMGUI
	ParticleSettings* settings = GetEditingSettings(scene);

	TransformData* emitterTransform = GetEditingTransform(scene);

	ImGui::SetNextWindowSize({420, 560}, ImGuiCond_FirstUseEver);

	if (ImGui::Begin("Particle Editor", isOpen)) {
		if (settings == nullptr) {
			ImGui::TextUnformatted("このシーンにはParticleがありません。");
		} else {
			ImGui::TextUnformatted("PARTICLE / 発生設定");

			if (targetId_ != kInvalidWorldObjectId) {
				ImGui::TextWrapped("選択中のParticleを編集しています。Ctrl+Sでシーンに保存します。");

				ImGui::TextWrapped("発生位置はTransformと共通のローカル値です。");
			}

			ImGui::TextWrapped("%s", ParticlePresetFile::kDefaultPath);

			if (targetId_ == kInvalidWorldObjectId) {
				if (IsDirty(scene)) {
					ImGui::TextUnformatted("* 未保存の変更");
				}
			}

			ImGui::TextWrapped("Scene内でプレビューします。ゲーム停止中にも調整できます。");

			ImGui::BeginDisabled(isGamePlaying);

			if (!preview_.isEnabled) {
				if (ImGui::Button("プレビュー開始")) {
					preview_.isEnabled = true;

					preview_.isPaused = false;

					preview_.isAutomaticEmissionEnabled = true;

					scene.ResetParticlePreview();

					isFocusRequested_ = true;
				}
			} else {
				if (ImGui::Button("プレビュー停止")) {
					preview_.isEnabled = false;

					scene.ResetParticlePreview();
				}
			}

			ImGui::SameLine();

			if (ImGui::Button("一時停止 / 再開")) {
				preview_.isPaused = !preview_.isPaused;
			}

			if (ImGui::Button("一度だけ発生")) {
				preview_.isEnabled = true;

				preview_.isPaused = false;

				preview_.isAutomaticEmissionEnabled = false;

				scene.BurstParticles();

				isFocusRequested_ = true;
			}

			ImGui::SameLine();

			if (ImGui::Button("リセット")) {
				scene.ResetParticlePreview();
			}

			ImGui::BeginDisabled(!preview_.isEnabled || !preview_.isPaused);

			if (ImGui::Button("1フレーム進める")) {
				preview_.isStepRequested = true;
			}

			ImGui::EndDisabled();

			ImGui::Checkbox("Particleだけを表示", &preview_.shouldIsolateParticles);

			if (ImGui::Button("発生位置へカメラを移動")) {
				isFocusRequested_ = true;
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

			if (ImGui::Button("プリセット")) {
				ImGui::OpenPopup("ParticlePresets");
			}

			if (ImGui::BeginPopup("ParticlePresets")) {
				if (ImGui::MenuItem("炎 / Fire")) {
					pendingAction_ = PresetAction::Fire;
				}

				if (ImGui::MenuItem("煙 / Smoke")) {
					pendingAction_ = PresetAction::Smoke;
				}

				if (ImGui::MenuItem("火花 / Sparks")) {
					pendingAction_ = PresetAction::Sparks;
				}

				ImGui::Separator();

				if (ImGui::MenuItem("既定値")) {
					pendingAction_ = PresetAction::Defaults;
				}

				ImGui::EndPopup();
			}

			if (!shouldOpenPrompt_) {
				if (pendingAction_ != PresetAction::None) {
					if (targetId_ == kInvalidWorldObjectId) {
						if (IsDirty(scene)) {
							ImGui::OpenPopup("Particleの未保存変更");

							shouldOpenPrompt_ = true;
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

						shouldOpenPrompt_ = false;

						ImGui::CloseCurrentPopup();
					}
				}

				if (ImGui::Button("破棄して続行")) {
					ApplyPendingAction(scene);

					shouldOpenPrompt_ = false;

					ImGui::CloseCurrentPopup();
				}

				if (ImGui::Button("キャンセル")) {
					pendingAction_ = PresetAction::None;

					shouldOpenPrompt_ = false;

					ImGui::CloseCurrentPopup();
				}

				ImGui::EndPopup();
			}

			DrawEmitterSettings(*settings, emitterTransform);

			DrawMotionSettings(*settings);

			DrawAppearanceSettings(*settings);

			DrawLifetimeSettings(*settings);

			ImGui::EndDisabled();

			if (!AreParticleSettingsValid(*settings)) {
				ImGui::TextWrapped("最小値と最大値などを確認してください。不正な設定は実行・保存しません。");
			}

			if (!status_.empty()) {
				ImGui::TextWrapped("%s", status_.c_str());
			}
		}
	}

	ImGui::End();

#else
	(void)isGamePlaying;

	(void)isOpen;

#endif
	scene.SetParticlePreview(preview_);

	preview_.isStepRequested = false;
}

// 閉じた補助画面のプレビューが、見えないまま走り続けないようにする。
void ParticleEditorPanel::StopPreview(IEditableWorld& scene, bool isGamePlaying) {
	if (preview_.isEnabled) {
		preview_.isEnabled = false;

		preview_.isStepRequested = false;

		// 再生中のゲームにある粒子は、補助画面を閉じても消さない。
		if (!isGamePlaying) {
			scene.ResetParticlePreview();
		}
	}

	scene.SetParticlePreview(preview_);
}

//=============================================================================================//
// Getter / 設定・注視位置

// 実行中の粒子数ではなく、保存対象の設定だけで未保存状態を判定する。
bool ParticleEditorPanel::IsDirty(IEditableWorld& scene) const {
	// 保存済みの設定と現在の設定を比較し、未保存の変更を調べる。
	const ParticleSettings* settings = scene.GetParticleSettings();

	if (scene_ != &scene) {
		return false;
	}

	if (settings == nullptr) {
		return false;
	}

	if (!AreParticleSettingsEqual(*settings, savedSettings_)) {
		return true;
	}

	return false;
}

Vector3 ParticleEditorPanel::GetFocusPosition() const {
	// SceneViewで注視する位置を取得する。
	return focusPosition_;
}

//=============================================================================================//
// Asset参照

void ParticleEditorPanel::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	savedSettings_.texture = change.RemapResource(savedSettings_.texture);
}

//=============================================================================================//
// 保存・注視要求

// 保存成功時だけ比較基準を更新し、失敗した設定を保存済み扱いにしない。
bool ParticleEditorPanel::Save(IEditableWorld& scene) {
	// プリセットの保存に成功した場合だけ、未保存判定の基準を更新する。
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

// 注視要求はSceneパネル側が一度だけ消費する。
bool ParticleEditorPanel::ConsumeFocusRequest() {
	// 注視要求を一度だけ取り出し、次のフレームへ持ち越さない。
	const bool isRequested = isFocusRequested_;

	isFocusRequested_ = false;

	return isRequested;
}

//=============================================================================================//
// Getter / 編集設定

// 選択がParticleの場合はそのComponentを編集し、それ以外では共通プリセットを編集する。
ParticleSettings* ParticleEditorPanel::GetEditingSettings(IEditableWorld& scene) const {
	// 選択中のEmitterまたは共通プリセットの編集設定を取得する。
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.id == targetId_) {
			if (object.type == WorldObjectType::Particle) {
				return object.particleSettings;
			}
		}
	}

	return scene.GetParticleSettings();
}

// 選択中のEmitterだけがTransformを発生位置として使う。
TransformData* ParticleEditorPanel::GetEditingTransform(IEditableWorld& scene) const {
	// 選択中のEmitterまたは共通プリセットのTransformを取得する。
	for (size_t objectIndex = 0; objectIndex < scene.GetEditableObjectCount(); ++objectIndex) {
		const EditableObjectView object = scene.GetEditableObject(objectIndex);

		if (object.id == targetId_) {
			if (object.type == WorldObjectType::Particle) {
				return object.transform;
			}
		}
	}

	return nullptr;
}

//=============================================================================================//
// プリセット反映

// 設定変更の破棄が確認できた操作だけを実行する。
void ParticleEditorPanel::ApplyPendingAction(IEditableWorld& scene) {
	ParticleSettings* settings = GetEditingSettings(scene);

	if (settings == nullptr) {
		return;
	}

	if (pendingAction_ == PresetAction::Load) {
		if (ParticlePresetFile::Load(*settings, ParticlePresetFile::kDefaultPath, status_)) {
			if (targetId_ == kInvalidWorldObjectId) {
				savedSettings_ = *settings;
			}

			status_ = "プリセットを読み込みました。";

			scene.ResetParticlePreview();
		}
	} else if (pendingAction_ == PresetAction::Defaults) {
		*settings = ParticleSettings{};

		scene.ResetParticlePreview();

		status_ = "既定値に戻しました。保存すると次回も使用できます。";
	} else {
		ParticlePresetKind kind = ParticlePresetKind::Fire;

		if (pendingAction_ == PresetAction::Smoke) {
			kind = ParticlePresetKind::Smoke;
		} else if (pendingAction_ == PresetAction::Sparks) {
			kind = ParticlePresetKind::Sparks;
		}

		const Vector3 emitterPosition = settings->emitterPosition;

		*settings = MakeParticlePreset(kind);

		settings->emitterPosition = emitterPosition; // プリセット選択で配置を移動しない。

		scene.ResetParticlePreview();

		status_ = "プリセットを適用しました。プレビューで確認し、必要に応じて保存してください。";
	}

	pendingAction_ = PresetAction::None;
}

// 個別Emitterのプリセット書出しでは、共通設定の保存基準を更新しない。
bool ParticleEditorPanel::SaveEditingPreset(IEditableWorld& scene) {
	// 編集対象のParticle設定を共通プリセットへ書き出す。
	if (targetId_ == kInvalidWorldObjectId) {
		return Save(scene);
	}

	const ParticleSettings* settings = GetEditingSettings(scene);

	if (settings == nullptr) {
		return false;
	}

	const bool wasSaved = ParticlePresetFile::Save(*settings, ParticlePresetFile::kDefaultPath, status_);

	if (wasSaved) {
		status_ = "選択Emitterをプリセットへ書き出しました。配置はシーン保存で保存できます。";
	}

	return wasSaved;
}
