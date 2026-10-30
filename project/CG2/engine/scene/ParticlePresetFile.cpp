#include "ParticlePresetFile.h"
#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>

namespace {
	// 読込・書込で軸の並びを統一する。
	void WriteVector(std::ostream& output, const Vector3& value) { output << value.x << ' ' << value.y << ' ' << value.z << ' '; }
	void ReadVector(std::istream& input, Vector3& value) { input >> value.x >> value.y >> value.z; }
}

// 完成した一時ファイルだけを置換し、書込途中の失敗で既存のプリセットを失わない。
bool ParticlePresetFile::Save(const ParticleSettings& settings, const std::filesystem::path& path, std::string& error) {
	error.clear();
	if (!ValidateParticleSettings(settings)) {
		error = "Particle settings contain invalid values.";
		return false;
	}
	std::error_code fileError;
	if (!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path(), fileError);
	}
	if (fileError) {
		error = fileError.message();
		return false;
	}
	std::filesystem::path temporaryPath = path;
	temporaryPath += L".tmp";
	std::ofstream output(temporaryPath);
	output.imbue(std::locale::classic());
	output << std::setprecision(std::numeric_limits<float>::max_digits10) << "PON_PARTICLE 1\n";
	WriteSettings(output, settings);
	output.close();
	if (!output) {
		error = "Particle preset could not be written.";
		std::filesystem::remove(temporaryPath, fileError);
		return false;
	}
	if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		error = "Particle preset could not be written.";
		std::filesystem::remove(temporaryPath, fileError);
		return false;
	}
	return true;
}

// 一旦別の設定へ読み込み、検証を通った場合だけ編集中の設定を置き換える。
bool ParticlePresetFile::Load(ParticleSettings& settings, const std::filesystem::path& path, std::string& error) {
	error.clear();
	std::ifstream input(path);
	input.imbue(std::locale::classic());
	std::string identifier;
	int version = 0;
	input >> identifier >> version;
	ParticleSettings loaded;
	if (identifier != "PON_PARTICLE" || version != 1) {
		error = "Unsupported particle preset format.";
		return false;
	}
	if (!ReadSettings(input, loaded)) {
		error = "Invalid particle preset format.";
		return false;
	}
	input >> std::ws;
	if (input.bad() || !input.eof()) {
		error = "Unexpected data after particle settings.";
		return false;
	}
	if (!ValidateParticleSettings(loaded)) {
		error = "Invalid particle preset values.";
		return false;
	}
	settings = loaded;
	return true;
}

// シーン内のEmitterも同じ順序で保存し、プリセットと設定項目を共有する。
void ParticlePresetFile::WriteSettings(std::ostream& output, const ParticleSettings& settings) {
	WriteVector(output, settings.emitterPosition);
	WriteVector(output, settings.spread);
	WriteVector(output, settings.velocityMinimum);
	WriteVector(output, settings.velocityMaximum);
	WriteVector(output, settings.particleScale);
	WriteVector(output, settings.acceleration);
	output << settings.color.x << ' ' << settings.color.y << ' ' << settings.color.z << ' ' << settings.color.w << '\n';
	output << settings.spawnCount << ' ' << settings.interval << ' ' << settings.lifetimeMinimum << ' ' << settings.lifetimeMaximum << ' '
	<< settings.randomColor << ' ' << settings.billboard << ' ' << settings.accelerationEnabled << ' '
		<< static_cast<int>(settings.blendMode) << ' ' << std::quoted(settings.texture) << '\n';
	}

// 一時値を検証してから反映するため、不完全な入力でも呼出元の設定は変えない。
bool ParticlePresetFile::ReadSettings(std::istream& input, ParticleSettings& settings) {
	ParticleSettings loaded;
	int randomColor = 0;
	int billboard = 0;
	int acceleration = 0;
	int blendMode = 0;
	ReadVector(input, loaded.emitterPosition);
	ReadVector(input, loaded.spread);
	ReadVector(input, loaded.velocityMinimum);
	ReadVector(input, loaded.velocityMaximum);
	ReadVector(input, loaded.particleScale);
	ReadVector(input, loaded.acceleration);
	input >> loaded.color.x >> loaded.color.y >> loaded.color.z >> loaded.color.w;
	input >> loaded.spawnCount >> loaded.interval >> loaded.lifetimeMinimum >> loaded.lifetimeMaximum >>
	randomColor >> billboard >> acceleration >> blendMode >> std::quoted(loaded.texture);
	if (!input) {
		return false;
	}
	if (randomColor < 0 || randomColor > 1) {
		return false;
	}
	if (billboard < 0 || billboard > 1) {
		return false;
	}
	if (acceleration < 0 || acceleration > 1) {
		return false;
	}
	if (blendMode < 0 || blendMode >= kCountOfBlendMode) {
		return false;
	}
	loaded.randomColor = randomColor != 0;
	loaded.billboard = billboard != 0;
	loaded.accelerationEnabled = acceleration != 0;
	loaded.blendMode = static_cast<BlendMode>(blendMode);
	if (!ValidateParticleSettings(loaded)) {
		return false;
	}
	settings = loaded;
	return true;
}
