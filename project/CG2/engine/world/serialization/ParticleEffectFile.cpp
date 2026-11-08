#include "serialization/ParticleEffectFile.h"
#include "serialization/ParticlePresetFile.h"
#include <Windows.h>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <utility>

namespace {
	// SceneとEffectで同じ順序を使い、ノードのTransformを9個のfloatとして保存する。
	void WriteTransform(std::ostream& output, const TransformData& transform) {
		output << transform.scale.x << ' ' << transform.scale.y << ' ' << transform.scale.z << ' '
			<< transform.rotate.x << ' ' << transform.rotate.y << ' ' << transform.rotate.z << ' '
			<< transform.translate.x << ' ' << transform.translate.y << ' ' << transform.translate.z << ' ';
	}

	bool ReadTransform(std::istream& input, TransformData& transform) {
		input >> transform.scale.x >> transform.scale.y >> transform.scale.z
			>> transform.rotate.x >> transform.rotate.y >> transform.rotate.z
			>> transform.translate.x >> transform.translate.y >> transform.translate.z;
		return static_cast<bool>(input);
	}

	bool ReplaceFile(const std::filesystem::path& temporaryPath, const std::filesystem::path& path) {
		// 書込み完了済みの一時ファイルだけを本体へ置き換える。
		return MoveFileExW(temporaryPath.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
	}

	// 旧ParticlePresetの共通項目の後ろに、新しいEffect専用設定を追記する。
	void WriteExtensions(std::ostream& output, const ParticleSettings& settings) {
		output << settings.isLifetimeCurveEnabled << ' ' << static_cast<uint32_t>(settings.emissionShape) << ' '
			<< settings.shapeRadius << ' ' << settings.coneAngle << ' ' << settings.isGroundCollisionEnabled << ' '
			<< settings.collisionHeight << ' ' << settings.collisionRestitution << ' ' << settings.isTrailEnabled << ' '
			<< settings.trailInterval << ' ' << settings.trailLifetime << ' ' << settings.trailWidth << '\n';
		for (const auto& key : settings.lifetimeCurve) {
			output << key.time << ' ' << key.size << ' ' << key.color.x << ' ' << key.color.y << ' ' << key.color.z << ' ' << key.color.w << '\n';
		}
	}

	bool ReadExtensions(std::istream& input, ParticleSettings& settings) {
		// フラグとenumを範囲確認してから設定へ戻し、壊れた値を文書に混ぜない。
		int curve = 0, collision = 0, trail = 0;
		uint32_t shape = 0;
		input >> curve >> shape >> settings.shapeRadius >> settings.coneAngle >> collision >> settings.collisionHeight
			>> settings.collisionRestitution >> trail >> settings.trailInterval >> settings.trailLifetime >> settings.trailWidth;
		if (!input || (curve != 0 && curve != 1) || (collision != 0 && collision != 1) || (trail != 0 && trail != 1) || shape > 3) return false;
		settings.isLifetimeCurveEnabled = curve != 0;
		settings.emissionShape = static_cast<ParticleEmissionShape>(shape);
		settings.isGroundCollisionEnabled = collision != 0;
		settings.isTrailEnabled = trail != 0;
		for (auto& key : settings.lifetimeCurve) input >> key.time >> key.size >> key.color.x >> key.color.y >> key.color.z >> key.color.w;
		return static_cast<bool>(input);
	}
}

bool ParticleEffectFile::Save(const ParticleEffectDocument& document, const std::filesystem::path& path, std::string& error) {
	// 保存前に文書全体を検証し、失敗時は既存ファイルを維持する。
	error.clear();
	if (!IsParticleEffectDocumentValid(document)) {
		error = "Particle effect contains invalid nodes or settings.";
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
	output << std::setprecision(std::numeric_limits<float>::max_digits10);
	WriteDocument(output, document);
	output.close();
	if (!output) {
		error = "Particle effect could not be written.";
		std::filesystem::remove(temporaryPath, fileError);
		return false;
	}
	if (!ReplaceFile(temporaryPath, path)) {
		error = "Particle effect could not be written.";
		std::filesystem::remove(temporaryPath, fileError);
		return false;
	}
	return true;
}

bool ParticleEffectFile::Load(ParticleEffectDocument& document, const std::filesystem::path& path, std::string& error) {
	// 旧単一Emitterと新しいノード文書を一つの編集モデルへ読み替える。
	error.clear();
	std::ifstream input(path);
	input.imbue(std::locale::classic());
	std::string identifier;
	int version = 0;
	input >> identifier >> version;

	ParticleEffectDocument loaded;
	if (identifier == "PON_PARTICLE") {
		// 既存のParticle 1/2は既定値で新項目を補い、ルートEmitterへ包む。
		ParticleSettings legacySettings;
		if (version < 1 || version > 2 || !ParticlePresetFile::ReadSettings(input, legacySettings, static_cast<unsigned>(version))) {
			error = "Invalid legacy particle preset.";
			return false;
		}
		input >> std::ws;
		if (input.bad() || !input.eof()) {
			error = "Unexpected data after legacy particle settings.";
			return false;
		}
		loaded = MakeParticleEffectDocument(legacySettings);
	} else if (identifier == "PON_PARTICLE_EFFECT") {
		// Effect v1には追加項目がないため、ParticleSettingsの既定値を使う。
		if (version < 1 || version > 2 || !ReadDocument(input, loaded, static_cast<unsigned>(version))) {
			error = "Invalid particle effect document.";
			return false;
		}
		input >> std::ws;
		if (input.bad() || !input.eof()) {
			error = "Unexpected data after particle effect document.";
			return false;
		}
	} else {
		error = "Unsupported particle effect format.";
		return false;
	}

	if (!IsParticleEffectDocumentValid(loaded)) {
		error = "Invalid particle effect values.";
		return false;
	}
	document = std::move(loaded);
	return true;
}

bool ParticleEffectFile::ReadFromStream(std::istream& input, ParticleEffectDocument& document) {
	// Sceneレコード内の連結データからEffect一件を読み、次のレコード位置を維持する。
	std::string identifier;
	unsigned version = 0;
	input >> identifier >> version;
	if (!input || identifier != "PON_PARTICLE_EFFECT" || !ReadDocument(input, document, version)) {
		return false;
	}
	return IsParticleEffectDocumentValid(document);
}

bool ParticleEffectFile::LoadSingleEmitter(ParticleSettings& settings, const std::filesystem::path& path, std::string& error) {
	// GameSceneの共通Particle用に、単一で平行移動だけのEffectを設定へ畳み込む。
	ParticleEffectDocument document;
	if (!Load(document, path, error)) {
		return false;
	}
	if (document.nodes.size() != 1 || document.nodes.front().type != ParticleNodeType::Emitter ||
		document.nodes.front().parentId != 0 || !document.nodes.front().isEnabled) {
		error = "This runtime can only load one enabled root emitter.";
		return false;
	}
	const TransformData& transform = document.nodes.front().transform;
	if (transform.scale.x != 1 || transform.scale.y != 1 || transform.scale.z != 1 ||
		transform.rotate.x != 0 || transform.rotate.y != 0 || transform.rotate.z != 0) {
		error = "The default runtime preset supports translation only.";
		return false;
	}
	settings = document.nodes.front().emitter;
	settings.emitterPosition = {settings.emitterPosition.x + transform.translate.x,
		settings.emitterPosition.y + transform.translate.y, settings.emitterPosition.z + transform.translate.z};
	return true;
}

void ParticleEffectFile::WriteDocument(std::ostream& output, const ParticleEffectDocument& document) {
	// 各Emitterは旧形式の基礎設定に続けてv2拡張値を持つ。Groupには設定行がない。
	output << "PON_PARTICLE_EFFECT 2\n" << std::quoted(document.name) << '\n' << document.nodes.size() << '\n';
	for (const ParticleEffectNode& node : document.nodes) {
		output << node.id << ' ' << node.parentId << ' ' << static_cast<uint32_t>(node.type) << ' ' << node.isEnabled << ' '
			<< std::quoted(node.name) << '\n';
		WriteTransform(output, node.transform);
		output << '\n';
		if (node.type == ParticleNodeType::Emitter) {
			ParticlePresetFile::WriteSettings(output, node.emitter);
			WriteExtensions(output, node.emitter);
		}
	}
}

bool ParticleEffectFile::ReadDocument(std::istream& input, ParticleEffectDocument& document, unsigned version) {
	// ノード数を上限内に制限し、全レコードが読めてから呼出元の文書を置き換える。
	if (version < 1 || version > 2) {
		return false;
	}
	ParticleEffectDocument loaded;
	uint32_t nodeCount = 0;
	input >> std::quoted(loaded.name) >> nodeCount;
	if (!input || nodeCount == 0 || nodeCount > kMaxParticleEffectNodes) {
		return false;
	}
	loaded.nodes.clear();
	loaded.nodes.reserve(nodeCount);
	for (uint32_t index = 0; index < nodeCount; ++index) {
		ParticleEffectNode node;
		uint32_t type = 0;
		int enabled = 0;
		input >> node.id >> node.parentId >> type >> enabled >> std::quoted(node.name);
		if (!input || (type != static_cast<uint32_t>(ParticleNodeType::Group) && type != static_cast<uint32_t>(ParticleNodeType::Emitter)) ||
			(enabled != 0 && enabled != 1) || !ReadTransform(input, node.transform)) {
			return false;
		}
		node.type = static_cast<ParticleNodeType>(type);
		node.isEnabled = enabled != 0;
		if (node.type == ParticleNodeType::Emitter) {
			// v1は基礎設定だけ、v2は基礎設定と拡張設定を読み取る。
			if (!ParticlePresetFile::ReadSettings(input, node.emitter, 2) || (version >= 2 && !ReadExtensions(input, node.emitter))) return false;
		}
		loaded.nodes.push_back(std::move(node));
	}
	if (!IsParticleEffectDocumentValid(loaded)) {
		return false;
	}
	document = std::move(loaded);
	return true;
}
