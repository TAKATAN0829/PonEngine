#pragma once

#include <filesystem>
#include <string>

class IEditableScene;

// Versioned text format. No dependency on editor UI or a concrete game scene.
class SceneFile {
public:
    static constexpr const char* kDefaultPath = "resources/scenes/GameScene.ponscene";
    static bool Save(IEditableScene& scene, const std::filesystem::path& path, std::string& error);
    static bool Load(IEditableScene& scene, const std::filesystem::path& path, std::string& error);
};
