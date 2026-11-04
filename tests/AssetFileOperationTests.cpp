#include "../project/CG2/editor/assets/AssetFileOperations.h"
#include "../project/CG2/editor/EditorSession.h"
#include "serialization/SceneFile.h"
#include "serialization/ParticlePresetFile.h"
#include "objects/WorldObjects.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <stdexcept>
#include <sstream>

namespace {
    void RequireFileOperation(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    void WriteTestAsset(const std::filesystem::path& path, const std::string& bytes) {
        std::filesystem::create_directories(path.parent_path());

        std::ofstream output(path, std::ios::binary);

        output << bytes;

        RequireFileOperation(static_cast<bool>(output), "Cannot write isolated asset fixture.");
    }

    std::string ReadTestAsset(const std::filesystem::path& path) {
        std::ifstream input(path, std::ios::binary);

        RequireFileOperation(static_cast<bool>(input), "Cannot read isolated asset fixture.");

        return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
    }

    class WorkingDirectoryScope {
    public:
        explicit WorkingDirectoryScope(const std::filesystem::path& directory) : original_(std::filesystem::current_path()) {
            std::filesystem::current_path(directory);
        }

        ~WorkingDirectoryScope() { std::filesystem::current_path(original_); }

    private:
        std::filesystem::path original_;
    };

    class AssetTestWorld : public WorldObjects {
    public:
        ParticleSettings* GetParticleSettings() override { return &shared; }

        void RemapAssetPaths(const AssetPathChange& change) override {
            WorldObjects::RemapAssetPaths(change);

            shared.texture = change.RemapResource(shared.texture);
        }

        ParticleSettings shared;
    };

    std::vector<AssetReference> LiveReferences(AssetTestWorld& world, const EditorSession& history) {
        std::vector<AssetReference> references;

        WorldAssetReferences::Collect(world.CaptureObjects(), "Current", references);

        history.CollectAssetReferences(references);

        references.push_back({AssetPathChange::Text(AssetPathChange::Path(world.shared.texture).lexically_relative("resources")), "Shared preset"});

        return references;
    }
}

// generated配下へ毎回独立したresourcesを作り、制作中のAssetを変更しない。
void RunAssetFileOperationTests(const std::filesystem::path& directory) {
    const std::filesystem::path root = std::filesystem::absolute(directory / "asset-operations" /
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) / "resources");

    WriteTestAsset(root / "images/source.png", "original image bytes");

    WriteTestAsset(root / "images/unrelated.png", "unrelated image bytes");

    WriteTestAsset(root / "models/item.obj", "mtllib ../materials/item.mtl\nv 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n");

    WriteTestAsset(root / "materials/item.mtl", "newmtl test\nmap_Kd ../images/source.png\n");

    WriteTestAsset(root / "data/mesh.bin", "binary buffer");

    WriteTestAsset(root / "gltf/model.gltf", "{\"asset\":{\"version\":\"2.0\"},\"name\":\"ignored\\nname\",\"buffers\":[{\"uri\":\"../data/mesh.bin\"}],\"images\":[{\"uri\":\"../images/source.png\"}]}\n");

    std::filesystem::create_directories(root / "destination");

    WorkingDirectoryScope workingDirectory(root.parent_path());

    AssetTestWorld world;

    world.shared.texture = "resources/images/source.png";

    WorldObjectData model;

    model.id = 1;

    model.name = "images/source.png"; // 同じ文字列のオブジェクト名を誤置換しない。

    model.model = "models/item.obj";

    model.prefabSource = "prefabs/item.ponprefab";

    model.materials.push_back({0, "images/source.png"});

    WorldObjectData particle;

    particle.id = 2;

    particle.type = WorldObjectType::Particle;

    particle.hasMeshRenderer = false;

    particle.particle.texture = "resources/images/source.png";

    std::string error;

    RequireFileOperation(world.ReplaceObjects({model, particle}, error), "Cannot create asset world.");

    RequireFileOperation(SceneFile::Save(world, root / "scenes/Example.ponscene", error), "Cannot save asset scene.");

    WorldObjects prefab;

    model.prefabSource.clear();

    RequireFileOperation(prefab.ReplaceObjects({model}, error), "Cannot create asset prefab.");

    RequireFileOperation(SceneFile::Save(prefab, root / "prefabs/item.ponprefab", error), "Cannot save asset prefab.");

    RequireFileOperation(ParticlePresetFile::Save(world.shared, root / "particles/Example.ponparticle", error), "Cannot save particle asset.");

    EditorSession history;

    history.Initialize(world);

    world.GetEditableObject(0).transform->translate.x = 2;

    history.Update(world, false);

    RequireFileOperation(history.Undo(world, error), "Cannot prepare redo state.");

    AssetFileOperations operations;

    RequireFileOperation(operations.Initialize(root, error), "Cannot initialize asset operations.");

    RequireFileOperation(operations.Review("images/source.png", LiveReferences(world, history), error), "Cannot inspect references.");

    RequireFileOperation(operations.GetUsages().size() >= 6, "Scene/prefab/particle/import/history references were not found.");

    std::filesystem::path trash;

    RequireFileOperation(!operations.MoveToTrash("images/source.png", LiveReferences(world, history), trash, error), "A used texture was deleted.");

    const std::string originalScene = ReadTestAsset(root / "scenes/Example.ponscene");

    // 旧形式のシーンを更新する場合も、従来の固定ライト相当を失わず形式13へ移す。
    std::istringstream oldSceneInput(ReadTestAsset(root / "prefabs/item.ponprefab"));

    std::string header;
    std::string count;
    std::string record;

    std::getline(oldSceneInput, header);

    std::getline(oldSceneInput, count);

    std::string legacyScene = "PON_SCENE 12\n" + count + "\n";

    while (std::getline(oldSceneInput, record)) {
        if (!record.empty()) {
            record.erase(record.rfind(' '));

            record.erase(record.rfind(' '));

            legacyScene += record + "\n";
        }
    }

    WriteTestAsset(root / "scenes/Legacy.ponscene", legacyScene);

    RequireFileOperation(!operations.Move("images/source.png", "images/unrelated.png", world, {}, error), "Existing destination was overwritten.");

    RequireFileOperation(ReadTestAsset(root / "scenes/Example.ponscene") == originalScene, "Rejected move changed scene bytes.");

    // 保存ファイルをロックして確定を失敗させ、Assetと既に書換えた参照の両方を復元する。
    const HANDLE lockedScene = CreateFileW((root / "scenes/Example.ponscene").c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

    RequireFileOperation(lockedScene != INVALID_HANDLE_VALUE, "Cannot lock isolated scene fixture.");

    const bool movedWhileLocked = operations.Move("images/source.png", "images/renamed.png", world, {}, error);

    CloseHandle(lockedScene);

    RequireFileOperation(!movedWhileLocked, "Move succeeded despite a locked reference file.");

    RequireFileOperation(std::filesystem::exists(root / "images/source.png") && !std::filesystem::exists(root / "images/renamed.png"), "Failed move did not restore original asset path.");

    RequireFileOperation(ReadTestAsset(root / "scenes/Example.ponscene") == originalScene, "Failed move changed original scene.");

    RequireFileOperation(world.GetObjectData(0)->materials.front().texture == "images/source.png", "Failed move changed live references.");

    const bool textureRenamed = operations.Move("images/source.png", "images/renamed.png", world, LiveReferences(world, history), error);

    if (!textureRenamed) {
        throw std::runtime_error("Referenced texture rename failed: " + error);
    }

    history.RemapAssetPaths({root, "images/source.png", "images/renamed.png"});

    RequireFileOperation(world.GetObjectData(0)->materials.front().texture == "images/renamed.png", "Live material reference was not remapped.");

    RequireFileOperation(world.GetObjectData(0)->name == "images/source.png", "Object name was mistaken for an asset reference.");

    WorldObjects migrated;

    RequireFileOperation(SceneFile::Load(migrated, root / "scenes/Legacy.ponscene", error), "Updated legacy scene cannot be loaded.");

    RequireFileOperation(migrated.GetEditableObjectCount() == 4, "Asset operation lost legacy default lights during schema upgrade.");

    RequireFileOperation(world.GetObjectData(1)->particle.texture == "resources/images/renamed.png", "Live emitter reference was not remapped.");

    RequireFileOperation(world.shared.texture == "resources/images/renamed.png", "Shared particle reference was not remapped.");

    RequireFileOperation(ReadTestAsset(root / "materials/item.mtl").find("../images/renamed.png") != std::string::npos, "MTL reference was not updated.");

    RequireFileOperation(ReadTestAsset(root / "gltf/model.gltf").find("../images/renamed.png") != std::string::npos, "glTF image URI was not updated.");

    WorldObjects restored;

    RequireFileOperation(SceneFile::Load(restored, root / "scenes/Example.ponscene", error), "Updated scene cannot be loaded.");

    RequireFileOperation(restored.GetObjectData(0)->materials.front().texture == "images/renamed.png", "Saved material reference was not updated.");

    RequireFileOperation(history.Redo(world, error), "Redo failed after asset rename.");

    RequireFileOperation(world.GetObjectData(0)->materials.front().texture == "images/renamed.png", "Redo restored a broken old reference.");

    RequireFileOperation(history.Undo(world, error), "Undo failed after asset rename.");

    RequireFileOperation(!history.IsDirty(world), "Asset rename changed the saved-state comparison.");

    const bool modelFolderMoved = operations.Move("models", "destination/models", world, LiveReferences(world, history), error);

    if (!modelFolderMoved) {
        throw std::runtime_error("Model folder move failed: " + error);
    }

    history.RemapAssetPaths({root, "models", "destination/models"});

    RequireFileOperation(world.GetObjectData(0)->model == "destination/models/item.obj", "Model folder move did not remap live objects.");

    RequireFileOperation(ReadTestAsset(root / "destination/models/item.obj").find("../../materials/item.mtl") != std::string::npos, "Moved OBJ did not preserve external MTL location.");

    RequireFileOperation(operations.Move("prefabs/item.ponprefab", "destination/item.ponprefab", world, {}, error), "Prefab move failed.");

    RequireFileOperation(world.GetObjectData(0)->prefabSource == "destination/item.ponprefab", "Prefab source link was not updated.");

    RequireFileOperation(operations.Move("data/mesh.bin", "destination/mesh.bin", world, {}, error), "External glTF buffer move failed.");

    RequireFileOperation(ReadTestAsset(root / "gltf/model.gltf").find("../destination/mesh.bin") != std::string::npos, "glTF buffer URI was not updated.");

    RequireFileOperation(operations.Move("images/renamed.png", "images/RENAMED.png", world, {}, error), "Case-only rename failed.");

    RequireFileOperation(!operations.Move("destination", "destination/child/nested", world, {}, error), "Folder was moved into itself.");

    RequireFileOperation(!operations.Move("images/RENAMED.png", "../escape.png", world, {}, error), "Move escaped resources.");

    RequireFileOperation(!operations.CreateFolder(root / "NUL", error), "Windows reserved filename was accepted.");

    RequireFileOperation(!operations.CreateFolder(root / ".editor-trash/hidden", error), "Recovery directory was editable.");

    RequireFileOperation(operations.CreateFolder(root / AssetPathChange::Path("新規フォルダ"), error), "UTF-8 folder creation failed.");

    RequireFileOperation(operations.MoveToTrash("images/unrelated.png", {}, trash, error), "Unused asset could not be moved to trash.");

    RequireFileOperation(!std::filesystem::exists(root / "images/unrelated.png") && ReadTestAsset(trash) == "unrelated image bytes", "Trash did not retain original bytes.");

    WriteTestAsset(root / "materials/options.mtl", "map_Kd -s 1 1 -o 0 0 0 ../images/RENAMED.png\n");

    std::string glbJson = "{\"asset\":{\"version\":\"2.0\"},\"images\":[{\"uri\":\"../images/RENAMED.png\"}]}";

    while (glbJson.size() % 4 != 0) {
        glbJson.push_back(' ');
    }

    const uint32_t fields[] = {0x46546c67, 2, static_cast<uint32_t>(20 + glbJson.size() + 12), static_cast<uint32_t>(glbJson.size()), 0x4e4f534a};

    std::string glb;

    for (uint32_t field : fields) {
        for (size_t byteIndex = 0; byteIndex < 4; ++byteIndex) {
            glb.push_back(static_cast<char>((field >> (8 * byteIndex)) & 255));
        }
    }

    const std::string binaryChunk("\x04\x00\x00\x00\x42\x49\x4e\x00" "DATA", 12);

    WriteTestAsset(root / "gltf/external.glb", glb + glbJson + binaryChunk);

    RequireFileOperation(operations.Move("images/RENAMED.png", "images/final name.png", world, {}, error), "Texture rename with spaces failed.");

    RequireFileOperation(ReadTestAsset(root / "gltf/external.glb").ends_with(binaryChunk), "GLB binary chunk was changed during URI remapping.");

    AssetDocument glbDocument;

    RequireFileOperation(glbDocument.Load(root, "gltf/external.glb", error), "Updated GLB header or JSON length is invalid.");

    RequireFileOperation(ReadTestAsset(root / "gltf/external.glb").find("final%20name.png") != std::string::npos, "GLB URI was not percent encoded.");

    RequireFileOperation(ReadTestAsset(root / "materials/options.mtl").find("-s 1 1 -o 0 0 0") != std::string::npos, "MTL texture options were modified.");

    WriteTestAsset(root / AssetPathChange::Path("images/日本語.png"), "unicode image bytes");

    WriteTestAsset(root / "gltf/unicode.gltf", "{\"images\":[{\"uri\":\"../images/\\u65e5\\u672c\\u8a9e.png\"}]}");

    RequireFileOperation(operations.Move(AssetPathChange::Path("images/日本語.png"), AssetPathChange::Path("images/日本語2.png"), world, {}, error), "Escaped Unicode glTF URI rename failed.");

    AssetDocument unicodeDocument;

    RequireFileOperation(unicodeDocument.Load(root, "gltf/unicode.gltf", error), "Updated Unicode URI cannot be read.");

    RequireFileOperation(unicodeDocument.GetReferences().front().path == "images/日本語2.png", "Unicode URI did not round-trip.");

    WriteTestAsset(root / "shaders/Builtin.hlsl", "shader fixture");

    RequireFileOperation(!operations.MoveToTrash("shaders", {}, trash, error), "Engine shader folder was deleted.");

    // 似た名前の隣のフォルダを巻き込まない。
    const AssetPathChange boundary{root, "images", "moved-images"};

    RequireFileOperation(boundary.RemapRelative("images-other/asset.png") == "images-other/asset.png", "Path prefix matched outside its folder boundary.");
}
