#pragma once

#include "objects/WorldObjectData.h"
#include "AssetPathChange.h"

// 保存・履歴・実行データが同じ規則でAsset参照を列挙・更新するための共通処理。
class WorldAssetReferences {
public:
	//=============================================================================================//
	// 参照の列挙

	static void Collect(const std::vector<WorldObjectData>& objects, const std::string& owner,
		std::vector<AssetReference>& references);

	//=============================================================================================//
	// 参照の更新

	static void Remap(std::vector<WorldObjectData>& objects, const AssetPathChange& change);
};
