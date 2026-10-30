#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "EngineStructs.h"
#include "ParticleSettings.h"

using SceneObjectId = uint64_t;
inline constexpr SceneObjectId kInvalidSceneObjectId = 0;

// 描画中のカメラと表示領域。Editorは描画クラスを直接参照せず、この情報で画面操作を行う。
struct EditorViewInfo {
    Matrix4x4 viewProjection{};
    Vector3 cameraPosition{};
    float viewportWidth = 0;
    float viewportHeight = 0;
};

// 保存・履歴に使う編集用データ。描画用のGPUリソースはGameScene側で別に管理する。
struct SceneObjectData {
    SceneObjectId id = kInvalidSceneObjectId;
    std::string name = "Model";
    std::string model = "plane.gltf";
    TransformData transform = {{1, 1, 1}, {}, {}};
    Vector4 color = {1, 1, 1, 1};
    bool enableLighting = true;
    float shininess = 40;
    bool useRootNodeMatrix = true;
    bool active = true; // 無効でも設定は残し、描画対象からだけ外す。
    bool hasMeshRenderer = true; // Componentを外してもモデルや色の設定は保持する。
};

// シーンが所有する値への借用参照。追加・削除・読込の後は取得し直す必要がある。
// UI側から破棄せず、フレームをまたいで保持しない。ImGuiには依存しない。
struct EditableObjectView {
    SceneObjectId id = kInvalidSceneObjectId;
    const char* name = "";
    TransformData* transform = nullptr;
    Vector4* color = nullptr;
    bool* enableLighting = nullptr;
    float* shininess = nullptr;
    std::string* model = nullptr;
    bool* useRootNodeMatrix = nullptr;
    std::string* editableName = nullptr;
    bool* active = nullptr;
    bool* hasMeshRenderer = nullptr;
};

// Editorに公開する操作の窓口。ゲーム側がパネルの具体的な型を知る必要はない。
class IEditableScene {
public:
    virtual ~IEditableScene() = default;
    virtual size_t GetEditableObjectCount() const = 0;
    virtual EditableObjectView GetEditableObject(size_t index) = 0;
    // カメラやモデルを持たないシーンでは、画面上の編集機能を利用しない。
    virtual bool GetEditorViewInfo(EditorViewInfo&) const { return false; }
    virtual void SetEditorViewport(uint32_t width, uint32_t height) { (void)width; (void)height; }
    virtual void SetEditorCameraInput(bool enabled) { (void)enabled; }
    virtual void SetEditorGameView(bool enabled) { (void)enabled; }
    virtual ParticleSettings* GetParticleSettings() { return nullptr; }
    virtual void SetParticlePreview(const ParticlePreviewState& state) { (void)state; }
    virtual void ResetParticlePreview() {}
    virtual void BurstParticles() {}
    virtual size_t GetParticleCount() const { return 0; }
    virtual bool GetEditableObjectBounds(size_t, AABB&) const { return false; }
    virtual void FocusEditorCamera(const Vector3& position) { (void)position; }
    virtual bool SupportsObjectCollection() const { return false; }
    virtual SceneObjectId AddObject(const std::string&) { return kInvalidSceneObjectId; }
    virtual SceneObjectId DuplicateObject(SceneObjectId) { return kInvalidSceneObjectId; }
    virtual bool RemoveObject(SceneObjectId) { return false; }
    virtual std::vector<SceneObjectData> CaptureObjects() const { return {}; }
    // 実装側は全データを先に検証し、失敗した場合は現在のシーンを変更しないこと。
    virtual bool ReplaceObjects(std::vector<SceneObjectData>, std::string& error) {
        error = "This scene does not support object replacement.";
        return false;
    }
};
