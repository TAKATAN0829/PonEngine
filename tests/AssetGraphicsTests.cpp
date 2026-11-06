#include "AssetGraphicsTests.h"
#include "GameScene.h"
#include "ModelManager.h"
#include "TextureManager.h"
#include "../project/CG2/editor/assets/AssetFileOperations.h"
#include "../project/CG2/editor/assets/AssetThumbnailCache.h"
#include "../project/CG2/editor/state/EditorSession.h"
#include <chrono>
#include <stdexcept>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/material.h>
#include <assimp/postprocess.h>

void RunAssetGraphicsTests(GameScene& scene) {
    // 実行時resourcesはRunEditorGraphicsTestsがgeneratedへ複製したものだけを使う。
    const std::filesystem::path resources = std::filesystem::canonical("resources");
    const std::filesystem::path directory = std::filesystem::path("asset-operation-tests") /
        std::to_string(std::chrono::system_clock::now().time_since_epoch().count());

    const std::filesystem::path source = directory / "Original";
    const std::filesystem::path destination = directory / "Moved";

    std::filesystem::create_directories(resources / directory);

    std::filesystem::copy(resources / "fence", resources / source, std::filesystem::copy_options::recursive);

    const std::string originalModel = AssetPathChange::Text(source / "fence.obj");
    const std::string movedModel = AssetPathChange::Text(destination / "fence.obj");
    const std::string originalTexture = "resources/" + AssetPathChange::Text(source / "fence.png");
    const std::string movedTexture = "resources/" + AssetPathChange::Text(destination / "fence.png");

    std::string error;

    if (!scene.PrepareModel(originalModel, error)) {
        throw std::runtime_error("Asset move GPU model preparation failed: " + error);
    }

    const WorldObjectId identifier = scene.AddObject(originalModel);

    if (identifier == kInvalidWorldObjectId) {
        throw std::runtime_error("Asset move GPU object creation failed.");
    }

    scene.GetEditableObject(scene.GetEditableObjectCount() - 1).transform->translate = {20, 0, 0};

    // Projectと同じ取得窓口へ、まず3Dモデルを依頼する。
    AssetThumbnailCache thumbnails;

    thumbnails.BeginFrame();

    const AssetThumbnail modelThumbnail = thumbnails.GetThumbnail(resources / source / "fence.obj", resources, ".obj", scene, error);

    // 新しい画像は次のフレームまで待たせ、モデルと画像が同じ準備上限を使うことを確認する。
    if (thumbnails.GetThumbnail(resources / source / "fence.png", resources, ".png", scene, error).handle != 0) {
        throw std::runtime_error("Asset thumbnails exceeded the shared frame budget.");
    }

    thumbnails.BeginFrame();

    const AssetThumbnail imageThumbnail = thumbnails.GetThumbnail(resources / source / "fence.png", resources, ".png", scene, error);

    if (imageThumbnail.handle == 0) {
        throw std::runtime_error("Image thumbnail preparation failed: " + error);
    }

    if (imageThumbnail.width != 990 || imageThumbnail.height != 360) {
        throw std::runtime_error("Image thumbnail did not preserve the source dimensions.");
    }

    const uint64_t textureBefore = TextureManager::GetInstance()->GetSrvHandleGPU(originalTexture).ptr;

    Model* modelBefore = ModelManager::GetInstance()->FindModel(originalModel);

    // 移動後と比較できるよう、元のモデルと正方形のサムネイルを確保する。
    if (modelThumbnail.handle == 0 || modelBefore == nullptr) {
        throw std::runtime_error("Asset move GPU preview preparation failed.");
    }

    if (modelThumbnail.width != 128 || modelThumbnail.height != 128) {
        throw std::runtime_error("Model thumbnail is not 128 pixels square.");
    }

    // 準備上限に達したフレームでも、取得済みのサムネイルは返る。
    if (thumbnails.GetThumbnail(resources / source / "fence.obj", resources, ".obj", scene, error).handle != modelThumbnail.handle) {
        throw std::runtime_error("Thumbnail cache did not reuse the model preview.");
    }

    // 移動前から実行用Object3dとプレビューが存在する状態にする。
    scene.GameScene::UpdateEditorFrame(nullptr, false);

    EditorSession history;

    history.Initialize(scene);

    scene.GetEditableObject(scene.GetEditableObjectCount() - 1).transform->translate.x = 21;

    history.Update(scene, false);

    AssetFileOperations operations;

    if (!operations.Initialize(resources, error)) {
        throw std::runtime_error(error);
    }

    std::vector<AssetReference> references;

    history.CollectAssetReferences(references);

    if (!operations.Move(source, destination, scene, references, error)) {
        throw std::runtime_error("GPU asset folder move failed: " + error);
    }

    history.RemapAssetPaths({resources, source, destination});

    if (ModelManager::GetInstance()->FindModel(movedModel) != modelBefore) {
        throw std::runtime_error("Asset move replaced the existing GPU model.");
    }

    if (TextureManager::GetInstance()->GetSrvHandleGPU(movedTexture).ptr != textureBefore) {
        throw std::runtime_error("Asset move replaced the existing texture descriptor.");
    }

    // Projectの一覧を更新した後も、移動先から同じGPUハンドルを受け取れる。
    thumbnails.Clear();

    thumbnails.BeginFrame();

    if (thumbnails.GetThumbnail(resources / destination / "fence.obj", resources, ".obj", scene, error).handle != modelThumbnail.handle) {
        throw std::runtime_error("Asset move lost the existing preview descriptor.");
    }

    thumbnails.BeginFrame();

    if (thumbnails.GetThumbnail(resources / destination / "fence.png", resources, ".png", scene, error).handle != imageThumbnail.handle) {
        throw std::runtime_error("Asset move lost the existing image thumbnail descriptor.");
    }

    if (!history.Undo(scene, error)) {
        throw std::runtime_error("GPU asset move Undo failed: " + error);
    }

    if (scene.GetObjectDataById(identifier)->model != movedModel) {
        throw std::runtime_error("GPU asset move Undo restored an old path.");
    }

    const std::string finalTexture = "resources/" + AssetPathChange::Text(destination / AssetPathChange::Path("Fence 日本語.png"));

    if (!operations.Move(destination / "fence.png", destination / AssetPathChange::Path("Fence 日本語.png"), scene, references, error)) {
        throw std::runtime_error("GPU texture rename failed: " + error);
    }

    if (TextureManager::GetInstance()->GetSrvHandleGPU(finalTexture).ptr != textureBefore) {
        throw std::runtime_error("Texture rename replaced its descriptor.");
    }

    // キャッシュではなく移動先のOBJ/MTLも再インポートできることを確認する。
    Assimp::Importer importer;

    const aiScene* imported = importer.ReadFile(AssetPathChange::Text(resources / destination / "fence.obj"), aiProcess_Triangulate);

    if (imported == nullptr) {
        throw std::runtime_error("Moved OBJ could not be reimported from disk.");
    }

    if (imported->mNumMeshes == 0) {
        throw std::runtime_error("Moved OBJ has no mesh after reimport.");
    }

    bool doesImportedTextureExist = false;

    for (uint32_t materialIndex = 0; materialIndex < imported->mNumMaterials; ++materialIndex) {
        aiString texture;

        if (imported->mMaterials[materialIndex]->GetTexture(aiTextureType_DIFFUSE, 0, &texture) == AI_SUCCESS) {
            if (std::filesystem::exists(resources / destination / AssetPathChange::Path(texture.C_Str()))) {
                doesImportedTextureExist = true;
            }
        }
    }

    if (!doesImportedTextureExist) {
        throw std::runtime_error("Moved OBJ/MTL texture reference could not be resolved by Assimp.");
    }

    std::filesystem::copy_file(resources / destination / "fence.obj", resources / destination / "reimport.obj");

    const std::string reimportedModel = AssetPathChange::Text(destination / "reimport.obj");

    if (!scene.PrepareModel(reimportedModel, error)) {
        throw std::runtime_error("Reimporting a Unicode texture name failed: " + error);
    }

    const std::vector<ModelMaterialInfo> reimportedMaterials = scene.GetModelMaterials(reimportedModel);

    bool wasReimportedTextureFound = false;

    for (const ModelMaterialInfo& material : reimportedMaterials) {
        if (material.texture == finalTexture) {
            wasReimportedTextureFound = true;
        }
    }

    if (!wasReimportedTextureFound) {
        throw std::runtime_error("Unicode texture metadata changed during model reimport.");
    }

    // GPUキャッシュがInspectorへ公開する情報も、新しいパスと一致する。
    const std::vector<ModelMaterialInfo> materials = scene.GetModelMaterials(movedModel);

    bool wasMovedTextureFound = false;

    for (const ModelMaterialInfo& material : materials) {
        if (material.texture == finalTexture) {
            wasMovedTextureFound = true;
        }
    }

    if (!wasMovedTextureFound) {
        throw std::runtime_error("Imported material metadata retained its old texture path.");
    }
}
