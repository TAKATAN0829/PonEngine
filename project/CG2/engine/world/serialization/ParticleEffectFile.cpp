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
		// 書き出した順と同じく、拡縮・回転・位置をそれぞれXYZの順で読む。
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
		for (const ParticleLifetimeKey& key : settings.lifetimeCurve) {
			output << key.time << ' ' << key.size << ' ' << key.color.x << ' ' << key.color.y << ' ' << key.color.z << ' ' << key.color.w << '\n';
		}
	}

	bool ReadExtensions(std::istream& input, ParticleSettings& settings) {
		// フラグとenumを範囲確認してから設定へ戻し、壊れた値を文書に混ぜない。
		int curve = 0, collision = 0, trail = 0;
		uint32_t shape = 0;
		// 形式2で増えたEmitter設定を、固定の項目順で読み取る。
		input >> curve >> shape >> settings.shapeRadius >> settings.coneAngle >> collision >> settings.collisionHeight
			>> settings.collisionRestitution >> trail >> settings.trailInterval >> settings.trailLifetime >> settings.trailWidth;
		if (!input || (curve != 0 && curve != 1) || (collision != 0 && collision != 1) || (trail != 0 && trail != 1) || shape > 3) return false;
		settings.isLifetimeCurveEnabled = curve != 0;
		settings.emissionShape = static_cast<ParticleEmissionShape>(shape);
		settings.isGroundCollisionEnabled = collision != 0;
		settings.isTrailEnabled = trail != 0;
		// 寿命曲線の各点は、時刻・大きさ・RGBAの組として並ぶ。
		for (ParticleLifetimeKey& key : settings.lifetimeCurve) input >> key.time >> key.size >> key.color.x >> key.color.y >> key.color.z >> key.color.w;
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
	// 指定されたパスをテキストとして開く。
	std::ifstream input(path);
	// 保存時と同じ形式で小数点を解釈する。
	input.imbue(std::locale::classic());
	std::string identifier;
	int version = 0;
	// ファイル冒頭の形式名を見て、単一Emitterとノード形式のどちらで読むか決める。
	input >> identifier >> version;

	// 途中で読み込みに失敗しても編集中の内容に影響しないよう、一時文書を用意する。
	ParticleEffectDocument loaded;
	if (identifier == "PON_PARTICLE") {
		// 既存のParticle 1/2は既定値で新項目を補い、ルートEmitterへ包む。
		ParticleSettings legacySettings;
		if (version < 1 || version > 2 || !ParticlePresetFile::ReadSettings(input, legacySettings, static_cast<unsigned>(version))) {
			error = "Invalid legacy particle preset.";
			return false;
		}
		// 一つのファイルには設定一件だけ。末尾の空白を飛ばしたら終端になることを確かめる。
		input >> std::ws;
		if (input.bad() || !input.eof()) {
			error = "Unexpected data after legacy particle settings.";
			return false;
		}
		// 旧形式の設定を、一つのルートEmitterを持つ文書に変換する。
		loaded = MakeParticleEffectDocument(legacySettings);
	} else if (identifier == "PON_PARTICLE_EFFECT") {
		// Effect v1には追加項目がないため、ParticleSettingsの既定値を使う。
		if (version < 1 || version > 3 || !ReadDocument(input, loaded, static_cast<unsigned>(version))) {
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

	// 各値が読めても、親子関係やノード構成が不正なら編集中の文書に反映しない。
	if (!IsParticleEffectDocumentValid(loaded)) {
		error = "Invalid particle effect values.";
		return false;
	}
	// 読み取りと文書全体の検証が済んでから、呼び出し元へ渡す。
	document = std::move(loaded);
	return true;
}

bool ParticleEffectFile::ReadFromStream(std::istream& input, ParticleEffectDocument& document) {
	// Sceneレコード内の連結データからEffect一件を読み、次のレコード位置を維持する。
	std::string identifier;
	unsigned version = 0;
	// ここでの入力元はSceneファイル。Effectの後ろに次のデータが続くため、終端までは読まない。
	input >> identifier >> version;
	if (!input || identifier != "PON_PARTICLE_EFFECT" || !ReadDocument(input, document, version)) {
		return false;
	}
	return IsParticleEffectDocumentValid(document);
}

void ParticleEffectFile::WriteDocument(std::ostream& output, const ParticleEffectDocument& document) {
	// v3ではノードの配置位置も保存し、各Emitterは設定と拡張値を続けて持つ。
	output << "PON_PARTICLE_EFFECT 3\n" << std::quoted(document.name) << '\n' << document.nodes.size() << '\n';
	for (const ParticleEffectNode& node : document.nodes) {
		output << node.id << ' ' << node.parentId << ' ' << static_cast<uint32_t>(node.type) << ' ' << node.isEnabled << ' '
			<< std::quoted(node.name) << '\n';
		WriteTransform(output, node.transform);
		int hasEditorPosition = 0;
		if (node.hasEditorPosition) {
			hasEditorPosition = 1;
		}
		output << hasEditorPosition << ' ' << node.editorPosition.x << ' ' << node.editorPosition.y << '\n';
		if (node.type == ParticleNodeType::Emitter) {
			ParticlePresetFile::WriteSettings(output, node.emitter);
			WriteExtensions(output, node.emitter);
		}
	}
}

bool ParticleEffectFile::ReadDocument(std::istream& input, ParticleEffectDocument& document, unsigned version) {
	// ノード数を上限内に制限し、全レコードが読めてから呼出元の文書を置き換える。
	if (version < 1 || version > 3) {
		return false;
	}
	ParticleEffectDocument loaded;
	uint32_t nodeCount = 0;
	// 名前は空白を含められる。先にノード数を検証し、その件数だけの領域を確保する。
	input >> std::quoted(loaded.name) >> nodeCount;
	if (!input || nodeCount == 0 || nodeCount > kMaxParticleEffectNodes) {
		return false;
	}
	loaded.nodes.clear();
	// 上限を確認した件数で、ノードを追加する前に容量だけを確保する。
	loaded.nodes.reserve(nodeCount);
	for (uint32_t index = 0; index < nodeCount; ++index) {
		ParticleEffectNode node;
		uint32_t type = 0;
		int enabled = 0;
		// 各ノードのID・親ID・種類・有効フラグ・名前を読み、続いてTransformを読む。
		input >> node.id >> node.parentId >> type >> enabled >> std::quoted(node.name);
		if (!input || (type != static_cast<uint32_t>(ParticleNodeType::Group) && type != static_cast<uint32_t>(ParticleNodeType::Emitter)) ||
			(enabled != 0 && enabled != 1) || !ReadTransform(input, node.transform)) {
			return false;
		}
		// 対応する番号だと確認できてから、ノードの種類を列挙型にする。
		node.type = static_cast<ParticleNodeType>(type);

		// 0/1で保存された値を、ノードの有効状態に変換する。
		node.isEnabled = enabled != 0;
		if (version >= 3) {
			int hasEditorPosition = 0;
			input >> hasEditorPosition >> node.editorPosition.x >> node.editorPosition.y;
			if (!input) {
				return false;
			}
			if (hasEditorPosition != 0 && hasEditorPosition != 1) {
				return false;
			}
			node.hasEditorPosition = hasEditorPosition != 0;
		}
		if (node.type == ParticleNodeType::Emitter) {
			// v1は基礎設定、v2以降は拡張設定も読み取る。
			if (!ParticlePresetFile::ReadSettings(input, node.emitter, 2) || (version >= 2 && !ReadExtensions(input, node.emitter))) return false;
		}
		// このノードの全項目が読めてから、一時文書に追加する。
		loaded.nodes.push_back(std::move(node));
	}
	document = std::move(loaded);
	return true;
}
