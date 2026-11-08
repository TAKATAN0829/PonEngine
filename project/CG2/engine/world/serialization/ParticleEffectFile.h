#pragma once

#include "settings/ParticleEffect.h"
#include <filesystem>
#include <iosfwd>

// ノード階層を持つParticleエフェクト文書の保存・読込。
class ParticleEffectFile {
public:
	static constexpr const char* kDefaultPath = "resources/particles/Default.ponparticle";

	static bool Save(const ParticleEffectDocument& document, const std::filesystem::path& path, std::string& error);
	static bool Load(ParticleEffectDocument& document, const std::filesystem::path& path, std::string& error);
	static bool LoadSingleEmitter(ParticleSettings& settings, const std::filesystem::path& path, std::string& error);
	static bool ReadFromStream(std::istream& input, ParticleEffectDocument& document);

	static void WriteDocument(std::ostream& output, const ParticleEffectDocument& document);
	static bool ReadDocument(std::istream& input, ParticleEffectDocument& document, unsigned version = 1);
};
