#pragma once

#include "ParticleSettings.h"
#include "EngineStructs.h"
#include "Vector2.h"
#include <cstdint>
#include <string>
#include <vector>

// エフェクト内で編集・保存するノードの種類。
enum class ParticleNodeType : uint32_t {
	Group,
	Emitter,
};

// 編集・保存用のノード。再生中のParticleやGPUリソースは保持しない。
struct ParticleEffectNode {
	ParticleEffectNode();

	uint64_t id;
	uint64_t parentId; // 0ならエフェクト直下。
	std::string name;
	ParticleNodeType type;
	bool isEnabled;
	Vector2 editorPosition;
	bool hasEditorPosition;
	TransformData transform;
	ParticleSettings emitter;
};

// UIやGPUに依存しないParticleエフェクト編集データ。
struct ParticleEffectDocument {
	std::string name;
	std::vector<ParticleEffectNode> nodes;
};

inline constexpr size_t kMaxParticleEffectNodes = 64;
inline constexpr size_t kMaxParticleEffectEmitters = 32;

// 既存の単一Emitter設定を、ルートEmitterひとつの文書へ包む。
ParticleEffectDocument MakeParticleEffectDocument(const ParticleSettings& settings, const std::string& name = "Particle Effect");

// 文書内で未使用の次のノードIDを返す。上限時は0を返す。
uint64_t GetNextParticleEffectNodeId(const ParticleEffectDocument& document);

// ノードID・親参照・設定値を検証する。
bool IsParticleEffectDocumentValid(const ParticleEffectDocument& document);

// Scene・Undoの変更比較でノード階層と設定値を比較する。
bool AreParticleEffectDocumentsEqual(const ParticleEffectDocument& first, const ParticleEffectDocument& second);
