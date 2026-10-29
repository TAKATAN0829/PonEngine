#pragma once

#include <filesystem>
#include <string>

class IEditableScene;

// バージョン付きのテキスト保存形式。EditorのUIや特定のGameSceneには依存しない。
class SceneFile {
public:
    static constexpr const char* kDefaultPath = "resources/scenes/GameScene.ponscene";
    // 書込失敗時は元の保存ファイルを残し、errorへ理由を返す。
    static bool Save(IEditableScene& scene, const std::filesystem::path& path, std::string& error);
    // 検証に成功したデータだけを反映し、不正ファイルで現在のシーンを壊さない。
    static bool Load(IEditableScene& scene, const std::filesystem::path& path, std::string& error);
};
