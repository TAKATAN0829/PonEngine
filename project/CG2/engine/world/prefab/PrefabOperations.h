#pragma once

#include "WorldObjectTypes.h"
#include "Vector3.h"
#include <filesystem>
#include <string>

class IWorldObjects;

// Prefabの保存・配置・Apply/Revertの手順を組み立てる。
// 配置データはWorldが所有し、ファイル形式の読書きはSceneFileへ任せる。
class PrefabOperations {
public:

	//=============================================================================================//
	// Prefab保存・配置

	// シーン形式を再利用し、マテリアル・アニメーション・種類別Componentも保存する。
	static bool Save(IWorldObjects& scene, WorldObjectId rootId, const std::filesystem::path& path, std::string& error);

	// 全件を検証してから新しいIDを割り当てる。失敗時にシーンを部分変更しない。
	static WorldObjectId Instantiate(IWorldObjects& scene, const std::filesystem::path& path,
		WorldObjectId parentId, const Vector3& position, std::string& error);

	// Projectなどのresources相対パスを検証し、配置を依頼する。
	static WorldObjectId Place(IWorldObjects& scene, const std::string& prefab,
		WorldObjectId parentId, const Vector3& position, std::string& error);

	//=============================================================================================//
	// 元Assetへの反映・復元

	// Applyはファイルを書き換えるため、シーンのUndoでは元Assetを戻せない。
	static bool Apply(IWorldObjects& scene, WorldObjectId rootId, std::string& error);

	// 復元時はルートのID・親・Transformを維持し、子孫は新しいIDで再構築する。
	static bool Revert(IWorldObjects& scene, WorldObjectId rootId, std::string& error);

	// resources内の元Assetだけを解決し、シンボリックリンクでの脱出も拒否する。
	static std::filesystem::path ResolveSource(const std::string& source, std::string& error);
};
