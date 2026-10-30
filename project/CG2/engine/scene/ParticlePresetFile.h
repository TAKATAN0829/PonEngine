#pragma once

#include "ParticleSettings.h"
#include <filesystem>
#include <iosfwd>

// Emitterの設定をプリセットとして保存する。配置と実行中の粒子は所有しない。
class ParticlePresetFile {
public:

	//=============================================================================================//
	// 定数 / 保存先

	static constexpr const char* kDefaultPath = "resources/particles/Default.ponparticle";

	//=============================================================================================//
	// メンバ関数 / ファイル保存・読込

	static bool Save(const ParticleSettings& settings, const std::filesystem::path& path, std::string& error);
	static bool Load(ParticleSettings& settings, const std::filesystem::path& path, std::string& error);

	//=============================================================================================//
	// メンバ関数 / 設定部分の保存・読込

	// シーン内Emitterも同じ項目順で保存し、読込は一時値を検証してから反映する。
	static void WriteSettings(std::ostream& output, const ParticleSettings& settings);
	static bool ReadSettings(std::istream& input, ParticleSettings& settings);
};
