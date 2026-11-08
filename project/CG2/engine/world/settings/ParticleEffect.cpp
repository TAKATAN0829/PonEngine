#include "settings/ParticleEffect.h"
#include <cmath>
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <utility>

ParticleEffectNode::ParticleEffectNode()
	: id(1),
	parentId(0),
	name("Emitter"),
	type(ParticleNodeType::Emitter),
	isEnabled(true),
	transform{{1,1,1},{},{}} {
	// 単一Emitterから始められる初期ノード値を用意する。
}

ParticleEffectDocument MakeParticleEffectDocument(const ParticleSettings& settings, const std::string& name) {
	// 旧設定をノード文書へ移すときの共通形として、ルートEmitterを一つ作る。
	ParticleEffectDocument document;
	document.name = name;
	ParticleEffectNode emitter;
	emitter.emitter = settings;
	document.nodes.push_back(emitter);
	return document;
}

uint64_t GetNextParticleEffectNodeId(const ParticleEffectDocument& document) {
	// 最大IDより一つ大きい値を使い、削除済みIDの再利用で参照を混同しない。
	uint64_t nextId = 1;
	for (const ParticleEffectNode& node : document.nodes) {
		if (node.id >= nextId) {
			if (node.id == (std::numeric_limits<uint64_t>::max)()) {
				return 0;
			}
			nextId = node.id + 1;
		}
	}
	return nextId;
}

bool IsParticleEffectDocumentValid(const ParticleEffectDocument& document) {
	// 参照検査用の索引を先に作り、ID重複・親欠落・循環を文書全体で判定する。
	if (document.name.empty() || document.name.size() > 256 || document.nodes.empty() ||
		document.nodes.size() > kMaxParticleEffectNodes) {
		return false;
	}

	std::unordered_map<uint64_t, const ParticleEffectNode*> nodesById;
	nodesById.reserve(document.nodes.size());
	bool hasEmitter = false;
	size_t emitterCount = 0;

	for (const ParticleEffectNode& node : document.nodes) {
		// ノードの局所値とEmitter設定を検査しながら、IDからノードへの索引を構築する。
		if (node.id == 0 || node.name.empty() || node.name.size() > 256 ||
			(static_cast<uint32_t>(node.type) != static_cast<uint32_t>(ParticleNodeType::Group) &&
			 static_cast<uint32_t>(node.type) != static_cast<uint32_t>(ParticleNodeType::Emitter)) ||
			!std::isfinite(node.transform.scale.x) || !std::isfinite(node.transform.scale.y) || !std::isfinite(node.transform.scale.z) ||
			!std::isfinite(node.transform.rotate.x) || !std::isfinite(node.transform.rotate.y) || !std::isfinite(node.transform.rotate.z) ||
			!std::isfinite(node.transform.translate.x) || !std::isfinite(node.transform.translate.y) || !std::isfinite(node.transform.translate.z) ||
			!nodesById.emplace(node.id, &node).second) {
			return false;
		}

		if (node.type == ParticleNodeType::Emitter && !AreParticleSettingsValid(node.emitter)) {
			return false;
		}
		hasEmitter = hasEmitter || node.type == ParticleNodeType::Emitter;
		if (node.type == ParticleNodeType::Emitter && ++emitterCount > kMaxParticleEffectEmitters) {
			return false;
		}
	}

	for (const ParticleEffectNode& node : document.nodes) {
		// 各親をルートまでたどり、同じIDの再訪を循環として拒否する。
		if (node.parentId != 0 && nodesById.find(node.parentId) == nodesById.end()) {
			return false;
		}
		if (node.parentId == node.id) {
			return false;
		}

		std::unordered_set<uint64_t> ancestors;
		uint64_t parentId = node.parentId;
		while (parentId != 0) {
			if (!ancestors.insert(parentId).second) {
				return false;
			}
			const auto parent = nodesById.find(parentId);
			if (parent == nodesById.end()) {
				return false;
			}
			parentId = parent->second->parentId;
		}
	}

	return hasEmitter;
}

bool AreParticleEffectDocumentsEqual(const ParticleEffectDocument& first, const ParticleEffectDocument& second) {
	// 同じ順序で保存される全ノードを比較し、階層や設定の編集を未保存差分に含める。
	if (first.name != second.name || first.nodes.size() != second.nodes.size()) {
		return false;
	}
	for (size_t index = 0; index < first.nodes.size(); ++index) {
		const ParticleEffectNode& a = first.nodes[index];
		const ParticleEffectNode& b = second.nodes[index];
		if (a.id != b.id || a.parentId != b.parentId || a.name != b.name || a.type != b.type || a.isEnabled != b.isEnabled ||
			a.transform.scale.x != b.transform.scale.x || a.transform.scale.y != b.transform.scale.y || a.transform.scale.z != b.transform.scale.z ||
			a.transform.rotate.x != b.transform.rotate.x || a.transform.rotate.y != b.transform.rotate.y || a.transform.rotate.z != b.transform.rotate.z ||
			a.transform.translate.x != b.transform.translate.x || a.transform.translate.y != b.transform.translate.y || a.transform.translate.z != b.transform.translate.z) {
			return false;
		}
		if (a.type == ParticleNodeType::Emitter && !AreParticleSettingsEqual(a.emitter, b.emitter)) {
			return false;
		}
	}
	return true;
}
