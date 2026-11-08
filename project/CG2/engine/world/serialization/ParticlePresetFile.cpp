#include "serialization/ParticlePresetFile.h"
#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>

//=============================================================================================//
// 実装内補助定義

namespace {
	// 読込・書込で軸の並びを統一する。
	void WriteVector(std::ostream& output, const Vector3& value) {
		// Vector3の三つの成分を、同じ順番で書き出す。
		output << value.x << ' ' << value.y << ' ' << value.z << ' ';
	}

	void ReadVector(std::istream& input, Vector3& value) {
		// 三つの成分を順番に読み込み、Vector3へ設定する。
		input >> value.x >> value.y >> value.z;
	}
}

//=============================================================================================//
// ファイル保存・読込


// 完成した一時ファイルだけを置換し、書込途中の失敗で既存のプリセットを失わない。
bool ParticlePresetFile::Save(const ParticleSettings& settings, const std::filesystem::path& path, std::string& error) {
	// 設定を検証し、一時ファイルへ書き終えてから保存先を置き換える。
	error.clear();

	// 不正な設定は書込前に拒否する。
	if (!AreParticleSettingsValid(settings)) {
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

	// 書き込みが途中で失敗しても既存のプリセットを壊さないよう、一時ファイルへ出力する。
	temporaryPath += L".tmp";

	std::ofstream output(temporaryPath);

	output.imbue(std::locale::classic());

	output << std::setprecision(std::numeric_limits<float>::max_digits10) << "PON_PARTICLE 2\n";

	WriteSettings(output, settings);

	output.close();

	if (!output) {
		error = "Particle preset could not be written.";

		std::filesystem::remove(temporaryPath, fileError);

		return false;
	}

	// 書き終えた一時ファイルだけを保存先へ置き換える。
	if (!MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
		error = "Particle preset could not be written.";

		std::filesystem::remove(temporaryPath, fileError);

		return false;
	}

	return true;
}

// 一旦別の設定へ読み込み、検証を通った場合だけ編集中の設定を置き換える。
bool ParticlePresetFile::Load(ParticleSettings& settings, const std::filesystem::path& path, std::string& error) {
	// 形式と値を検証し、成功した場合だけ設定を反映する。
	error.clear();

	// テキストとして開き、先頭の形式名と版番号で読み方を決める。
	std::ifstream input(path);

	// PCの地域設定に関係なく、保存時と同じ小数点の表記を使う。
	input.imbue(std::locale::classic());

	// 先頭にある形式名の文字列を受け取る。
	std::string identifier;

	// 形式名に続く版番号を受け取る。
	int version = 0;

	// ストリームから、形式名と版番号をファイルに並んでいる順で取り出す。
	input >> identifier >> version;

	// 編集中の設定とは別の領域で読込と検証を行う。
	ParticleSettings loaded;

	if (identifier != "PON_PARTICLE" || version < 1 || version > 2) {
		error = "Unsupported particle preset format.";

		return false;
	}

	// ヘッダーの続きから、確認した版に合う順序で設定本体を読む。
	if (!ReadSettings(input, loaded,version)) {
		error = "Invalid particle preset format.";

		return false;
	}

	// 末尾の空白は許可し、設定の後ろに別の値が残っていれば失敗にする。
	input >> std::ws;

	if (input.bad() || !input.eof()) {
		error = "Unexpected data after particle settings.";

		return false;
	}

	// 読み取った発生間隔や寿命などが、設定として使える範囲にあるか確認する。
	if (!AreParticleSettingsValid(loaded)) {
		error = "Invalid particle preset values.";

		return false;
	}

	// すべての検証が済んだ設定だけを反映する。
	settings = loaded;

	return true;
}

//=============================================================================================//
// 設定部分の保存・読込


// シーン内のEmitterも同じ順序で保存し、プリセットと設定項目を共有する。
void ParticlePresetFile::WriteSettings(std::ostream& output, const ParticleSettings& settings) {
	// 保存対象の設定値を、形式で決めた順番に書き出す。
	WriteVector(output, settings.emitterPosition);
	WriteVector(output, settings.spread);
	WriteVector(output, settings.velocityMinimum);
	WriteVector(output, settings.velocityMaximum);
	WriteVector(output, settings.particleScale);
	WriteVector(output, settings.acceleration);

	output << settings.color.x << ' ' << settings.color.y << ' ' << settings.color.z << ' ' << settings.color.w << '\n';

	output << settings.spawnCount << ' ' << settings.interval << ' ' << settings.lifetimeMinimum << ' ' << settings.lifetimeMaximum << ' '
	<< settings.isRandomColorEnabled << ' ' << settings.isBillboardEnabled << ' ' << settings.isAccelerationEnabled << ' '
		<< static_cast<int>(settings.blendMode) << ' ' << std::quoted(settings.texture) << '\n';

	output << settings.isLifetimeAppearanceEnabled << ' ' << settings.startSize << ' ' << settings.endSize << ' '
		<< settings.endColor.x << ' ' << settings.endColor.y << ' ' << settings.endColor.z << ' ' << settings.endColor.w << ' '
		<< settings.fadeIn << ' ' << settings.areSoftParticlesEnabled << ' ' << settings.softDistance << '\n';
}

// 一時値を検証してから反映するため、不完全な入力でも呼出元の設定は変えない。
bool ParticlePresetFile::ReadSettings(std::istream& input, ParticleSettings& settings, unsigned version) {
	// 保存形式に応じて設定値を読み取り、値の範囲を検証する。
	if (version < 1 || version > 2) {
		return false;
	}

	ParticleSettings loaded;

	int randomColor = 0;
	int billboard = 0;
	int acceleration = 0;
	int blendMode = 0;

	// 位置・速度などの三成分を、保存側と同じ順で読み進める。
	ReadVector(input, loaded.emitterPosition);
	ReadVector(input, loaded.spread);
	ReadVector(input, loaded.velocityMinimum);
	ReadVector(input, loaded.velocityMaximum);
	ReadVector(input, loaded.particleScale);
	ReadVector(input, loaded.acceleration);

	// 色は赤・緑・青・不透明度の4成分を順に読む。
	input >> loaded.color.x >> loaded.color.y >> loaded.color.z >> loaded.color.w;

	// フラグと合成方法は整数で受け取り、画像パスは引用符で囲まれた範囲を読む。
	input >> loaded.spawnCount >> loaded.interval >> loaded.lifetimeMinimum >> loaded.lifetimeMaximum >>
	randomColor >> billboard >> acceleration >> blendMode >> std::quoted(loaded.texture);

	// 途中で値が足りなかった場合や、数値に変換できなかった場合を検出する。
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

	// 0/1と確認したフラグをboolへ変え、合成方法の番号を列挙型へ変える。
	loaded.isRandomColorEnabled = randomColor != 0;
	loaded.isBillboardEnabled = billboard != 0;
	loaded.isAccelerationEnabled = acceleration != 0;
	loaded.blendMode = static_cast<BlendMode>(blendMode);

	// 形式1には外観とソフトパーティクルの追加項目がないため、形式2だけ続きを読む。
	if (version >= 2) {
		int lifetimeAppearance = 0;
		int softParticles = 0;

		// 寿命に応じた見た目の変化に続けて、背景と交差したときのフェード設定を読む。
		input >> lifetimeAppearance >> loaded.startSize >> loaded.endSize >> loaded.endColor.x >> loaded.endColor.y
			>> loaded.endColor.z >> loaded.endColor.w >> loaded.fadeIn >> softParticles >> loaded.softDistance;

		if (!input) {
			return false;
		}

		if (lifetimeAppearance < 0 || lifetimeAppearance > 1 || softParticles < 0 || softParticles > 1) {
			return false;
		}

		// 形式2の追加フラグを、読み取った整数からboolへ戻す。
		loaded.isLifetimeAppearanceEnabled = lifetimeAppearance != 0;

		loaded.areSoftParticlesEnabled = softParticles != 0;
	}

	if (!AreParticleSettingsValid(loaded)) {
		return false;
	}

	settings = loaded;

	return true;
}
