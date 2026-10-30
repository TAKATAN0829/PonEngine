#include "ParticleSettings.h"
#include <array>
#include <cmath>

// 各軸の最小値・最大値と有限値を確認し、発生数や寿命には実行可能な上限を設ける。
bool ValidateParticleSettings(const ParticleSettings& settings) {
    const std::array<float, 25> values = {
        settings.emitterPosition.x, settings.emitterPosition.y, settings.emitterPosition.z,
        settings.spread.x, settings.spread.y, settings.spread.z,
        settings.velocityMinimum.x, settings.velocityMinimum.y, settings.velocityMinimum.z,
        settings.velocityMaximum.x, settings.velocityMaximum.y, settings.velocityMaximum.z,
        settings.particleScale.x, settings.particleScale.y, settings.particleScale.z,
        settings.color.x, settings.color.y, settings.color.z, settings.color.w,
        settings.acceleration.x, settings.acceleration.y, settings.acceleration.z,
        settings.interval, settings.lifetimeMinimum, settings.lifetimeMaximum};
    for (float value : values) { if (!std::isfinite(value)) { return false; } }
    return settings.spawnCount >= 1 && settings.spawnCount <= 100 && settings.interval >= 0.01f && settings.interval <= 60 &&
        settings.lifetimeMinimum >= 0.01f && settings.lifetimeMaximum <= 60 && settings.lifetimeMinimum <= settings.lifetimeMaximum &&
        settings.spread.x >= 0 && settings.spread.y >= 0 && settings.spread.z >= 0 &&
        settings.particleScale.x > 0 && settings.particleScale.y > 0 && settings.particleScale.z > 0 &&
        settings.velocityMinimum.x <= settings.velocityMaximum.x && settings.velocityMinimum.y <= settings.velocityMaximum.y &&
        settings.velocityMinimum.z <= settings.velocityMaximum.z && settings.blendMode >= kBlendModeNone &&
        settings.blendMode < kCountOfBlendMode &&
        (settings.texture == "resources/circle.png" || settings.texture == "resources/uvChecker.png" || settings.texture == "resources/white1x1.png");
}

// 保存対象の設定だけを比較し、プレビュー中の粒子の変化では未保存表示を出さない。
bool EqualParticleSettings(const ParticleSettings& firstSettings, const ParticleSettings& secondSettings) {
    return firstSettings.emitterPosition.x == secondSettings.emitterPosition.x &&
        firstSettings.emitterPosition.y == secondSettings.emitterPosition.y &&
        firstSettings.emitterPosition.z == secondSettings.emitterPosition.z &&
        firstSettings.spread.x == secondSettings.spread.x && firstSettings.spread.y == secondSettings.spread.y && firstSettings.spread.z == secondSettings.spread.z &&
        firstSettings.velocityMinimum.x == secondSettings.velocityMinimum.x && firstSettings.velocityMinimum.y == secondSettings.velocityMinimum.y && firstSettings.velocityMinimum.z == secondSettings.velocityMinimum.z &&
        firstSettings.velocityMaximum.x == secondSettings.velocityMaximum.x && firstSettings.velocityMaximum.y == secondSettings.velocityMaximum.y && firstSettings.velocityMaximum.z == secondSettings.velocityMaximum.z &&
        firstSettings.particleScale.x == secondSettings.particleScale.x && firstSettings.particleScale.y == secondSettings.particleScale.y && firstSettings.particleScale.z == secondSettings.particleScale.z &&
        firstSettings.color.x == secondSettings.color.x && firstSettings.color.y == secondSettings.color.y && firstSettings.color.z == secondSettings.color.z && firstSettings.color.w == secondSettings.color.w &&
        firstSettings.acceleration.x == secondSettings.acceleration.x && firstSettings.acceleration.y == secondSettings.acceleration.y && firstSettings.acceleration.z == secondSettings.acceleration.z &&
        firstSettings.spawnCount == secondSettings.spawnCount && firstSettings.interval == secondSettings.interval &&
        firstSettings.lifetimeMinimum == secondSettings.lifetimeMinimum && firstSettings.lifetimeMaximum == secondSettings.lifetimeMaximum &&
        firstSettings.randomColor == secondSettings.randomColor && firstSettings.billboard == secondSettings.billboard &&
        firstSettings.accelerationEnabled == secondSettings.accelerationEnabled && firstSettings.blendMode == secondSettings.blendMode && firstSettings.texture == secondSettings.texture;
}
