#pragma once

#include <filesystem>
#include <string>

class IEditableScene;

// Editorに依存しない、バージョン付きのシーン保存形式を扱う。
class SceneFile {
public:

	//=============================================================================================//
	// 定数 / 保存先

	static constexpr const char* kDefaultPath = "resources/scenes/GameScene.ponscene";

	//=============================================================================================//
	// メンバ関数 / シーン保存

	// 書き終えた一時ファイルだけを置換し、失敗時は元ファイルを残す。
	static bool Save(IEditableScene& scene, const std::filesystem::path& path, std::string& error);

	//=============================================================================================//
	// メンバ関数 / シーン読込

	// 全件検証に成功した場合だけ、現在の編集データへ反映する。
	static bool Load(IEditableScene& scene, const std::filesystem::path& path, std::string& error);
};
