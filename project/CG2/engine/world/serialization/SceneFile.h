#pragma once

#include <filesystem>
#include <string>

class IWorldObjects;

// Editorに依存しない、バージョン付きのシーン保存形式を扱う。
class SceneFile {
public:

	//=============================================================================================//
	// 定数 / 保存先

	static constexpr const char* kDefaultPath = "resources/scenes/GameScene.ponscene";

	//=============================================================================================//
	// シーン保存

	// 書き終えた一時ファイルだけを置換し、失敗時は元ファイルを残す。
	// requireModelFiles=falseはAsset移動の一時ファイル準備専用。通常保存は存在も確認する。
	static bool Save(IWorldObjects& scene, const std::filesystem::path& path, std::string& error, bool requireModelFiles = true);

	//=============================================================================================//
	// シーン読込

	// 全件検証に成功した場合だけ、現在の編集データへ反映する。
	// Editor/ゲームの旧シーン読込では固定ライトを移行する。Prefab等の部分読込では追加しない。
	static bool Load(IWorldObjects& scene, const std::filesystem::path& path, std::string& error, bool migrateLegacyLights = false);
};
