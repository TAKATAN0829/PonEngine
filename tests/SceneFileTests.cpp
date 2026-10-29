#include "SceneFile.h"
#include "IEditableScene.h"

#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

class TestScene : public IEditableScene {
public:
    TransformData transforms[2] = {
        {{1, 2, 3}, {0.1f, 0.2f, 0.3f}, {4, 5, 6}},
        {{2, 3, 4}, {0.4f, 0.5f, 0.6f}, {7, 8, 9}}
    };
    Vector4 colors[2] = {{0.1f, 0.2f, 0.3f, 1}, {0.4f, 0.5f, 0.6f, 1}};
    bool lighting[2] = {true, false};
    float shininess[2] = {40, 80};
    size_t GetEditableObjectCount() const override { return 2; }
    EditableObjectView GetEditableObject(size_t i) override {
        return {static_cast<SceneObjectId>(i + 1), "Test", &transforms[i], &colors[i], &lighting[i], &shininess[i]};
    }
};
}

int main() {
    try {
        const std::filesystem::path directory = "generated/scene-file-tests/data";
        const auto path = directory / "roundtrip.ponscene";
        TestScene scene;
        std::string error;
        Require(SceneFile::Save(scene, path, error), "save failed");
        scene.transforms[0].translate.x = 123;
        scene.transforms[1].rotate.y = 456;
        scene.colors[0].x = 0;
        scene.lighting[0] = false;
        scene.shininess[1] = 1;
        Require(SceneFile::Load(scene, path, error), "load failed");
        Require(scene.transforms[0].translate.x == 4 && scene.transforms[1].rotate.y == 0.5f,
            "transform roundtrip failed");
        Require(scene.colors[0].x == 0.1f && scene.lighting[0] && scene.shininess[1] == 80,
            "appearance roundtrip failed");

        // Read both records so malformed second records can test transactional loading.
        std::ifstream input(path);
        std::string header, count, first, second;
        std::getline(input, header); std::getline(input, count);
        std::getline(input, first); std::getline(input, second);
        input.close();
        const auto broken = directory / "broken.ponscene";
        const auto reject = [&](const std::string& contents) {
            std::ofstream out(broken); out << contents; out.close();
            scene.transforms[0].translate.x = 999;
            Require(!SceneFile::Load(scene, broken, error), "bad file accepted");
            Require(!error.empty(), "missing error message");
            Require(scene.transforms[0].translate.x == 999, "failed load changed live data");
        };
        reject("PON_SCENE 99\n2\n" + first + "\n" + second);
        reject("PON_SCENE 1\n2\n" + first + "\n2 15 1");
        reject("PON_SCENE 1\n2\n" + first + "\n" + first);
        reject("PON_SCENE 1\n2\n" + first + "\n" + second + "\ntrailing");
        reject("PON_SCENE 1\n2\n" + first + "\n3" + second.substr(1));
        Require(!SceneFile::Load(scene, directory / "missing.ponscene", error), "missing file accepted");

        // A rejected save must preserve the previous good file.
        scene.shininess[0] = -1;
        Require(!SceneFile::Save(scene, path, error), "invalid save accepted");
        Require(SceneFile::Load(scene, path, error), "rejected save damaged existing file");
        Require(scene.shininess[0] == 40, "saved file changed");
        // Saving again exercises replacement of an existing destination.
        scene.transforms[0].translate.x = 42;
        Require(SceneFile::Save(scene, path, error), "replacement save failed");
        scene.transforms[0].translate.x = 0;
        Require(SceneFile::Load(scene, path, error) && scene.transforms[0].translate.x == 42,
            "replacement roundtrip failed");
        std::cout << "SceneFile tests passed\n";
        return 0;
    } catch (const std::exception& exception) {
        std::cerr << exception.what() << '\n';
        return 1;
    }
}
