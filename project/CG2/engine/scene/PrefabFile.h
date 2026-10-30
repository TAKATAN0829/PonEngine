#pragma once
#include "IEditableScene.h"
#include <filesystem>

// Prefabはルートと子孫を保存する。元Assetへの明示的なApply/Revertを提供する。
class PrefabFile {
public:
	//=============================================================================================//
	// 保存 / 読み込み・配置

	// シーン形式を再利用し、マテリアル・アニメーション・種類別Componentも保存する。
	static bool Save(IEditableScene& scene, SceneObjectId rootId, const std::filesystem::path& path, std::string& error);
	// 全件を検証してから新しいIDを割り当てる。失敗時にシーンを部分変更しない。
	static SceneObjectId Instantiate(IEditableScene& scene, const std::filesystem::path& path,
		SceneObjectId parentId, const Vector3& position, std::string& error);
	//=============================================================================================//
	// メンバ関数 / 元Assetへの反映・復元

	// Applyはファイルを書き換えるため、シーンのUndoでは元Assetを戻せない。
	static bool Apply(IEditableScene& scene, SceneObjectId rootId, std::string& error);
	// 復元時はルートのID・親・Transformを維持し、子孫は新しいIDで再構築する。
	static bool Revert(IEditableScene& scene, SceneObjectId rootId, std::string& error);
	// resources内の元Assetだけを解決し、シンボリックリンクでの脱出も拒否する。
	static std::filesystem::path ResolveSource(const std::string& source, std::string& error);
};
