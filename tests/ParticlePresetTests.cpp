#include "ParticlePresetFile.h"
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
// 失敗した条件を実行元へ返し、検証漏れを成功扱いにしない。
void RequireParticleCondition(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}
}

// 正常な往復保存と、不正ファイルで編集中の設定が変わらないことを確認する。
void RunParticlePresetTests() {
    const std::filesystem::path directory = "generated/scene-file-tests/particles";
    const std::filesystem::path presetPath = directory / "test.ponparticle";
    ParticleSettings settings;
    settings.emitterPosition = {1,2,3};
    settings.spread = {2,0.5f,3};
    settings.velocityMinimum = {-3,-2,-1};
    settings.velocityMaximum = {3,4,5};
    settings.particleScale = {0.3f,0.4f,0.5f};
    settings.color = {0.2f,0.4f,0.8f,0.6f};
    settings.acceleration = {0,-9.8f,0};
    settings.accelerationEnabled = true;
    settings.randomColor = true;
    settings.billboard = false;
    settings.spawnCount = 80;
    settings.interval = 0.03f;
    settings.lifetimeMinimum = 0.5f;
    settings.lifetimeMaximum = 5;
    settings.texture = "resources/uvChecker.png";
    settings.blendMode = kBlendModeScreen;
    const ParticleSettings original = settings;
    std::string error;
    RequireParticleCondition(ParticlePresetFile::Save(settings, presetPath, error), "Particle save failed");
    settings = {};
    RequireParticleCondition(ParticlePresetFile::Load(settings, presetPath, error) && EqualParticleSettings(settings, original),
        "Particle settings did not roundtrip");
    settings.spawnCount = 0;
    RequireParticleCondition(!ParticlePresetFile::Save(settings, presetPath, error), "Invalid emission count was saved");
    RequireParticleCondition(ParticlePresetFile::Load(settings, presetPath, error) && EqualParticleSettings(settings, original),
        "Rejected save damaged the existing preset");
    settings.velocityMinimum.x = 100;
    RequireParticleCondition(!ValidateParticleSettings(settings), "Inverted velocity range was accepted");
    settings = original;
    settings.lifetimeMinimum = 8;
    RequireParticleCondition(!ValidateParticleSettings(settings), "Inverted lifetime range was accepted");
    settings = original;
    settings.interval = std::numeric_limits<float>::quiet_NaN();
    RequireParticleCondition(!ValidateParticleSettings(settings), "Non-finite particle setting was accepted");

    std::ifstream source(presetPath);
    const std::string contents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
    source.close();
    const std::filesystem::path brokenPath = directory / "broken.ponparticle";
    std::ofstream broken(brokenPath);
    broken << "PON_PARTICLE 99\n";
    broken.close();
    settings = original;
    RequireParticleCondition(!ParticlePresetFile::Load(settings, brokenPath, error) && EqualParticleSettings(settings, original),
        "Rejected load changed particle settings");
    broken.open(brokenPath);
    broken << contents << "extra";
    broken.close();
    RequireParticleCondition(!ParticlePresetFile::Load(settings, brokenPath, error), "Trailing preset data was accepted");
    std::string unknownTexture = contents;
    unknownTexture.replace(unknownTexture.find("resources/uvChecker.png"), 23, "../unknown.png");
    broken.open(brokenPath);
    broken << unknownTexture;
    broken.close();
    RequireParticleCondition(!ParticlePresetFile::Load(settings, brokenPath, error) && EqualParticleSettings(settings, original),
        "Unknown particle texture was accepted");
    RequireParticleCondition(!ParticlePresetFile::Load(settings, directory / "missing", error), "Missing preset was accepted");
}
