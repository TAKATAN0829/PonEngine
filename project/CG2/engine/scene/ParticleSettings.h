#pragma once
#include "EngineStructs.h"
#include "../dx12/BlendMode.h"
#include <string>

// 編集・プリセット保存に使う設定。粒子の現在位置や経過時間は含めない。
struct ParticleSettings {
    Vector3 emitterPosition{};
    Vector3 spread = {1,1,1};
    Vector3 velocityMinimum = {-1,-1,-1};
    Vector3 velocityMaximum = {1,1,1};
    Vector3 particleScale = {1,1,1};
    Vector4 color = {1,0.65f,0.2f,1};
    Vector3 acceleration = {0,1,0};
    uint32_t spawnCount = 3;
    float interval = 0.1f;
    float lifetimeMinimum = 1;
    float lifetimeMaximum = 3;
    bool randomColor = false;
    bool billboard = true;
    bool accelerationEnabled = false;
    BlendMode blendMode = kBlendModeAdd;
    std::string texture = "resources/circle.png";
};

// プレビューの再生状態は設定ファイルへ保存せず、実行中の操作として扱う。
struct ParticlePreviewState {
    bool enabled = false;
    bool paused = false;
    bool step = false;
    bool isolate = false;
    bool automaticEmission = true;
};

// 保存前と読込後の両方で同じ検証を使い、不正な設定を実行時へ流さない。
bool ValidateParticleSettings(const ParticleSettings& settings);
bool EqualParticleSettings(const ParticleSettings& firstSettings, const ParticleSettings& secondSettings);
