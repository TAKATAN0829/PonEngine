#pragma once

#include "SceneObjectTypes.h"
#include <string>
#include <vector>
#include "EngineStructs.h"
#include "components/ParticleSettings.h"
#include "components/SceneAssetSettings.h"
#include "components/SceneMotionSettings.h"
#include "components/SceneColliderSettings.h"
#include "components/SceneBodySettings.h"
#include "components/SceneLightSettings.h"

// 保存する透視投影の設定。画角はラジアン、描画距離はワールド単位。
struct SceneCameraSettings {
	float fieldOfView = 0.785398163f;
	float nearClip = 0.1f;
	float farClip = 1000;
};

// ゲームカメラの計算結果。Editorの入力や画面状態は含めない。
struct SceneCameraView {
	Matrix4x4 cameraWorld{};
	Matrix4x4 viewProjection{};
};

// 保存・履歴に使う共有データ。描画用のGPUリソースはGameScene側で別に管理する。
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
	SceneCameraSettings camera;
	bool mainCamera = false; // メイン指定はSetMainCamera経由で一つだけにする。
	ParticleSettings particle; // 発生位置はTransformを使用し、それ以外の設定を個別に保持する。
	std::vector<SceneMaterialSettings> materials; // 空のスロットはモデル既定値を使用する。
	SceneAnimationSettings animation;
	// Prefabルートだけがresources相対の元Assetを保持する。子は独立した編集データ。
	std::string prefabSource;
	SceneMotionSettings motion; // ゲーム再生時だけTransformへ反映する動作設定。
	SceneColliderSettings collider; // モデルやボーン形状とは独立した接触範囲。
	SceneBodySettings body; // 質量・重力・接触特性。実行速度は保存しない。
	SceneLightSettings light; // Spotの内角・外角。角度は度数で保存する。
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
	SceneCameraSettings* camera = nullptr;
	bool mainCamera = false;
	std::vector<SceneMaterialSettings>* materials = nullptr;
	SceneAnimationSettings* animation = nullptr;
	SceneBodySettings* body = nullptr; // 実行中の速度への参照は公開しない。
	SceneColliderSettings* collider = nullptr; // 借用参照。構造変更後は取得し直す。
	SceneMotionSettings* motion = nullptr; // 借用参照。フレームをまたいで保持しない。
	SceneLightSettings* light = nullptr; // Light固有設定。構造変更後は取得し直す。
};

