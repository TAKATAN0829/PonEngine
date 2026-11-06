#include "objects/WorldAssetReferences.h"

//=============================================================================================//
// 参照の列挙

void WorldAssetReferences::Collect(const std::vector<WorldObjectData>& objects, const std::string& owner,
	std::vector<AssetReference>& references) {
	// 各オブジェクトのモデル・Material・Particleの参照先を列挙する。
	for (const WorldObjectData& object : objects) {
		const std::string location = owner + " / " + object.name;

		references.push_back({object.model, location + " / Model"});

		// Prefabのリンク先も利用中のAssetとして扱い、移動・削除時の検査に含める。
		if (!object.prefabSource.empty()) {
			references.push_back({object.prefabSource, location + " / Prefab"});
		}

		for (const MaterialSettings& material : object.materials) {
			if (!material.texture.empty()) {
				references.push_back({material.texture, location + " / Material " + std::to_string(material.slot)});
			}
		}

		if (object.type == WorldObjectType::Particle) {
			references.push_back({AssetPathChange::Text(AssetPathChange::Path(object.particle.texture).lexically_relative("resources")), location + " / Particle"});
		}
	}
}

//=============================================================================================//
// 参照の更新

void WorldAssetReferences::Remap(std::vector<WorldObjectData>& objects, const AssetPathChange& change) {
	// モデル・Material・Particleに保存したAssetパスを置き換える。
	for (WorldObjectData& object : objects) {
		object.model = change.RemapRelative(object.model);

		object.prefabSource = change.RemapRelative(object.prefabSource);

		for (MaterialSettings& material : object.materials) {
			material.texture = change.RemapRelative(material.texture);
		}

		object.particle.texture = change.RemapResource(object.particle.texture);
	}
}
