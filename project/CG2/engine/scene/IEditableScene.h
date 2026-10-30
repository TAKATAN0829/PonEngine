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
	Matrix4x4 viewProjection{}; // Sceneカメラのビュー射影行列。
	Vector3 cameraPosition{}; // ワールド座標でのカメラ位置。
	float viewportWidth = 0; // 画像上の投影とレイ判定に共通で使う幅。
	float viewportHeight = 0; // 同じ表示領域の高さ。
};

// 補助画面は必要なときだけ開く。Editorが所有し、描画呼出中だけシーンへ貸し出す。
struct EditorToolVisibility {
	bool particleEditor = false;
	bool worldSettings = false;
	bool camera = false;
};

// 保存・履歴に使う編集用データ。描画用のGPUリソースはGameScene側で別に管理する。
enum class SceneObjectType { Model, Empty, PointLight, Particle };

struct SceneObjectData {
	//=============================================================================================//
	// 識別情報

	SceneObjectId id = kInvalidSceneObjectId;
	SceneObjectId parentId = kInvalidSceneObjectId; // 0なら最上位。Transformは親に対するローカル値。
	std::string name = "Model";
	std::string model = "plane.gltf";
	//=============================================================================================//
	// Transform・Mesh Renderer

	TransformData transform = {{1, 1, 1}, {}, {}};
	Vector4 color = {1, 1, 1, 1};
	bool enableLighting = true;
	float shininess = 40;
	bool useRootNodeMatrix = true;
	bool active = true; // 無効でも設定は残し、描画対象からだけ外す。
	bool hasMeshRenderer = true; // Componentを外してもモデルや色の設定は保持する。
	//=============================================================================================//
	// 種類別Component

	SceneObjectType type = SceneObjectType::Model;
	float lightIntensity = 1;
	float lightRadius = 10;
	float lightDecay = 1;
	ParticleSettings particle; // 発生位置はTransformを使用し、それ以外の設定を個別に保持する。
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
	SceneObjectType type = SceneObjectType::Model;
	float* lightIntensity = nullptr;
	float* lightRadius = nullptr;
	float* lightDecay = nullptr;
	ParticleSettings* particleSettings = nullptr;
	SceneObjectId parentId = kInvalidSceneObjectId; // 変更はSetObjectParentを通して検証する。
};

// Editorに公開する操作の窓口。ゲーム側がパネルの具体的な型を知る必要はない。
class IEditableScene {
public:
	//=============================================================================================//
	// 終了処理

	virtual ~IEditableScene() = default;

	//=============================================================================================//
	// Getter / オブジェクト一覧

	virtual size_t GetEditableObjectCount() const = 0;
	// 返す参照は追加・削除・読込で無効になるため、フレームをまたいで保持しない。
	virtual EditableObjectView GetEditableObject(size_t index) = 0;
	// 履歴比較用の読み取り専用参照。一覧変更後には取得し直す。
	// 公開できないシーンはnullptrを返し、CaptureObjectsでの比較へ切り替える。
	virtual const SceneObjectData* GetObjectData(size_t index) const { (void)index; return nullptr; }
	// 親子変換は保存データと共通の行列で扱う。GPU側の参照は公開しない。
	virtual bool GetObjectWorldMatrix(SceneObjectId, Matrix4x4&) const { return false; }
	Vector3 GetObjectWorldPosition(SceneObjectId id) const {
		Matrix4x4 world{};
		if (!GetObjectWorldMatrix(id, world)) {
			return {};
		}
		return {world.m[3][0], world.m[3][1], world.m[3][2]};
	}
	virtual bool SetObjectWorldPosition(SceneObjectId, const Vector3&) { return false; }
	virtual bool IsObjectActive(SceneObjectId) const { return false; }
	virtual bool SetObjectParent(SceneObjectId, SceneObjectId, std::string& error) {
		error = "This scene does not support parenting.";
		return false;
	}
	virtual bool SupportsObjectCollection() const { return false; }
	virtual std::vector<SceneObjectData> CaptureObjects() const { return {}; }

	//=============================================================================================//
	// Getter / Sceneカメラ・選択範囲

	// 対応するカメラや形状を持たないシーンではfalseを返す。
	virtual bool GetEditorViewInfo(EditorViewInfo&) const { return false; }
	virtual bool GetEditableObjectBounds(size_t, AABB&) const { return false; }

	//=============================================================================================//
	// 描画処理 / 任意の補助画面

	virtual void DrawEditorTools(EditorToolVisibility& visibility) { (void)visibility; }

	//=============================================================================================//
	// Setter / Scene表示・入力

	virtual void SetEditorViewport(uint32_t width, uint32_t height) { (void)width; (void)height; }
	virtual void SetEditorCameraInput(bool enabled) { (void)enabled; }
	virtual void SetEditorGameView(bool enabled) { (void)enabled; }
	virtual void FocusEditorCamera(const Vector3& position) { (void)position; }

	//=============================================================================================//
	// Getter / Particle

	virtual ParticleSettings* GetParticleSettings() { return nullptr; }
	virtual size_t GetParticleCount() const { return 0; }

	//=============================================================================================//
	// Setter・メンバ関数 / Particleプレビュー

	virtual void SetParticlePreview(const ParticlePreviewState& state) { (void)state; }
	virtual void ResetParticlePreview() {}
	virtual void BurstParticles() {}

	//=============================================================================================//
	// メンバ関数 / オブジェクト操作

	virtual SceneObjectId AddObject(const std::string&) { return kInvalidSceneObjectId; }
	virtual SceneObjectId CreateObject(SceneObjectType) { return kInvalidSceneObjectId; }
	virtual SceneObjectId DuplicateObject(SceneObjectId) { return kInvalidSceneObjectId; }
	virtual bool RemoveObject(SceneObjectId) { return false; }
	// 実装側は全件を検証してから置換し、失敗時は現在の一覧を維持する。
	virtual bool ReplaceObjects(std::vector<SceneObjectData>, std::string& error) {
		error = "This scene does not support object replacement.";
		return false;
	}
};
