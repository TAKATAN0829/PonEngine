#pragma once
#include "ParticleSettings.h"
#include <filesystem>

// Particle Editorの設定をシーン配置とは別のプリセットとして保存する。
class ParticlePresetFile {
public:
    static constexpr const char* kDefaultPath = "resources/particles/Default.ponparticle";
    static bool Save(const ParticleSettings& settings, const std::filesystem::path& path, std::string& error);
    static bool Load(ParticleSettings& settings, const std::filesystem::path& path, std::string& error);
};
