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
    if (settings == nullptr) { return true; }
    if (!ParticlePresetFile::Save(*settings, ParticlePresetFile::kDefaultPath, status_)) {
        LogSystem::Log(status_ + "\n");
        return false;
    }
    savedSettings_ = *settings;
    status_ = "プリセットを保存しました。";
    LogSystem::Log("Particle preset saved.\n");
    return true;
}

// 設定変更の破棄が確認できた操作だけを実行する。
void ParticleEditorPanel::ApplyPendingAction(IEditableScene& scene) {
    ParticleSettings* settings = scene.GetParticleSettings();
    if (settings == nullptr) { return; }
    if (pendingAction_ == 1) {
        if (ParticlePresetFile::Load(*settings, ParticlePresetFile::kDefaultPath, status_)) {
            savedSettings_ = *settings;
            status_ = "プリセットを読み込みました。";
            scene.ResetParticlePreview();
        }
    } else if (pendingAction_ == 2) {
        *settings = ParticleSettings{};
        scene.ResetParticlePreview();
        status_ = "既定値に戻しました。保存すると次回も使用できます。";
    }
    pendingAction_ = 0;
}

// 注視要求はSceneパネル側が一度だけ消費する。
bool ParticleEditorPanel::ConsumeFocusRequest() {
    const bool requested = focusRequested_;
    focusRequested_ = false;
    return requested;
}

// Particle用の再生はゲームのPlay状態と分離し、編集中にもScene内で確認できるようにする。
void ParticleEditorPanel::Draw(IEditableScene& scene, bool gamePlaying) {
    ParticleSettings* settings = scene.GetParticleSettings();
    if (scene_ != &scene) {
        scene_ = &scene;
        if (settings != nullptr) { savedSettings_ = *settings; }
        preview_ = {};
        status_.clear();
    }
#ifdef USE_IMGUI
    if (ImGui::Begin("Particle Editor")) {
        if (settings == nullptr) {
            ImGui::TextUnformatted("このシーンにはParticleがありません。");
        } else {
            ImGui::TextUnformatted("PARTICLE / 発生設定");
            if (IsDirty(scene)) { ImGui::TextUnformatted("* 未保存の変更"); }
            ImGui::TextWrapped("Scene内でプレビューします。ゲーム停止中にも調整できます。");
            ImGui::BeginDisabled(gamePlaying);
            if (!preview_.enabled) {
                if (ImGui::Button("プレビュー開始")) { preview_.enabled = true; preview_.paused = false; preview_.automaticEmission = true; scene.ResetParticlePreview(); focusRequested_ = true; }
            } else {
                if (ImGui::Button("プレビュー停止")) { preview_.enabled = false; scene.ResetParticlePreview(); }
            }
            ImGui::SameLine();
            if (ImGui::Button("一時停止 / 再開")) { preview_.paused = !preview_.paused; }
            if (ImGui::Button("一度だけ発生")) { preview_.enabled = true; preview_.paused = false; preview_.automaticEmission = false; scene.BurstParticles(); focusRequested_ = true; }
            ImGui::SameLine();
            if (ImGui::Button("リセット")) { scene.ResetParticlePreview(); }
            ImGui::BeginDisabled(!preview_.enabled || !preview_.paused);
            if (ImGui::Button("1フレーム進める")) { preview_.step = true; }
            ImGui::EndDisabled();
            ImGui::Checkbox("Particleだけを表示", &preview_.isolate);
            if (ImGui::Button("発生位置へカメラを移動")) { focusRequested_ = true; }
            ImGui::Text("粒子数: %zu / 100", scene.GetParticleCount());
            ImGui::Separator();
            if (ImGui::Button("保存")) { Save(scene); }
            ImGui::SameLine();
            if (ImGui::Button("読込")) { pendingAction_ = 1; }
            ImGui::SameLine();
            if (ImGui::Button("既定値")) { pendingAction_ = 2; }
            if (pendingAction_ != 0 && !openPrompt_) {
                if (IsDirty(scene)) { ImGui::OpenPopup("Particleの未保存変更"); openPrompt_ = true; }
                else { ApplyPendingAction(scene); }
            }
            if (ImGui::BeginPopupModal("Particleの未保存変更", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("変更を保存してから設定を置き換えますか？");
                if (ImGui::Button("保存して続行")) {
                    if (Save(scene)) { ApplyPendingAction(scene); openPrompt_ = false; ImGui::CloseCurrentPopup(); }
                }
                if (ImGui::Button("破棄して続行")) { ApplyPendingAction(scene); openPrompt_ = false; ImGui::CloseCurrentPopup(); }
                if (ImGui::Button("キャンセル")) { pendingAction_ = 0; openPrompt_ = false; ImGui::CloseCurrentPopup(); }
                ImGui::EndPopup();
            }
            if (ImGui::CollapsingHeader("Emitter / 発生", ImGuiTreeNodeFlags_DefaultOpen)) {
                EditParticleVector("発生位置", settings->emitterPosition, -1000, 1000);
                EditParticleVector("拡散範囲", settings->spread, 0, 100);
                int spawnCount = static_cast<int>(settings->spawnCount);
                ImGui::SliderInt("一度の発生数", &spawnCount, 1, 100);
                settings->spawnCount = static_cast<uint32_t>(spawnCount);
                ImGui::DragFloat("発生間隔（秒）", &settings->interval, 0.01f, 0.01f, 60, "%.2f", ImGuiSliderFlags_AlwaysClamp);
                ImGui::DragFloatRange2("寿命（秒）", &settings->lifetimeMinimum, &settings->lifetimeMaximum,
                    0.05f, 0.01f, 60, "min %.2f", "max %.2f", ImGuiSliderFlags_AlwaysClamp);
            }
            if (ImGui::CollapsingHeader("Motion / 動き", ImGuiTreeNodeFlags_DefaultOpen)) {
                EditParticleVector("最小速度", settings->velocityMinimum, -100, 100);
                EditParticleVector("最大速度", settings->velocityMaximum, -100, 100);
                ImGui::Checkbox("加速度を使う", &settings->accelerationEnabled);
                EditParticleVector("加速度", settings->acceleration, -100, 100);
            }
            if (ImGui::CollapsingHeader("Appearance / 見た目", ImGuiTreeNodeFlags_DefaultOpen)) {
                EditParticleVector("粒子の大きさ", settings->particleScale, 0.01f, 100);
                ImGui::ColorEdit4("色", &settings->color.x);
                ImGui::Checkbox("ランダムな色", &settings->randomColor);
                ImGui::Checkbox("カメラへ向ける", &settings->billboard);
                const char* textures[] = {"resources/circle.png", "resources/uvChecker.png", "resources/white1x1.png"};
                int textureIndex = 0;
                for (int candidateIndex = 0; candidateIndex < 3; ++candidateIndex) {
                    if (settings->texture == textures[candidateIndex]) { textureIndex = candidateIndex; }
                }
                if (ImGui::Combo("画像", &textureIndex, textures, 3)) { settings->texture = textures[textureIndex]; }
                const char* blends[] = {"None", "Normal", "Add", "Subtract", "Multiply", "Screen", "Lighten", "Darken", "Invert"};
                int blendIndex = static_cast<int>(settings->blendMode);
                if (ImGui::Combo("合成", &blendIndex, blends, kCountOfBlendMode)) { settings->blendMode = static_cast<BlendMode>(blendIndex); }
            }
            ImGui::EndDisabled();
            if (!ValidateParticleSettings(*settings)) { ImGui::TextWrapped("最小値と最大値などを確認してください。不正な設定は実行・保存しません。"); }
            if (!status_.empty()) { ImGui::TextWrapped("%s", status_.c_str()); }
        }
    }
    ImGui::End();
#else
    (void)gamePlaying;
#endif
    scene.SetParticlePreview(preview_);
    preview_.step = false;
}
