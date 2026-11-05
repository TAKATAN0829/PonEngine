#pragma once

#include "WorldObjectTypes.h"
#include <string>
#include <vector>
#include "EngineStructs.h"
#include "settings/ParticleSettings.h"
#include "settings/AssetSettings.h"
#include "settings/MotionSettings.h"
#include "settings/ColliderSettings.h"
#include "settings/RigidBodySettings.h"
#include "settings/LightSettings.h"

// 保存する透視投影の設定。画角はラジアン、描画距離はワールド単位。
struct CameraSettings {
	float fieldOfView = 0.785398163f;
	float nearClip = 0.1f;
	float farClip = 1000;
};

// ゲームカメラの計算結果。Editorの入力や画面状態は含めない。
struct CameraView {
	Matrix4x4 cameraWorld{};
	Matrix4x4 viewProjection{};
};

// 保存・履歴に使う共有データ。描画用のGPUリソースはGameScene側で別に管理する。
struct WorldObjectData {
	//=============================================================================================//
	// 識別情報

	WorldObjectId id = kInvalidWorldObjectId;
	WorldObjectId parentId = kInvalidWorldObjectId; // 0なら最上位。Transformは親に対するローカル値。

	std::string name = "Model";
	std::string model = "plane.gltf";

	//=============================================================================================//
	// Transform・Mesh Renderer

	TransformData transform = {{1, 1, 1}, {}, {}};

	Vector4 color = {1, 1, 1, 1};

	bool isLightingEnabled = true;

	float shininess = 40;

	bool isRootNodeMatrixUsed = true;
	bool isActive = true; // 無効でも設定は残し、描画対象からだけ外す。
	bool hasMeshRenderer = true; // Componentを外してもモデルや色の設定は保持する。

	//=============================================================================================//
	// 種類別Component

	WorldObjectType type = WorldObjectType::Model;

	float lightIntensity = 1;
	float lightRadius = 10;
	float lightDecay = 1;

	CameraSettings camera;

	bool isMainCamera = false; // メイン指定はSetMainCamera経由で一つだけにする。

	ParticleSettings particle; // 発生位置はTransformを使用し、それ以外の設定を個別に保持する。

	std::vector<MaterialSettings> materials; // 空のスロットはモデル既定値を使用する。

	AnimationSettings animation;

	// Prefabルートだけがresources相対の元Assetを保持する。子は独立した編集データ。
	std::string prefabSource;

	MotionSettings motion; // ゲーム再生時だけTransformへ反映する動作設定。

	ColliderSettings collider; // モデルやボーン形状とは独立した接触範囲。

	RigidBodySettings body; // 質量・重力・接触特性。実行速度は保存しない。

	LightSettings light; // Spotの内角・外角。角度は度数で保存する。
};

// シーンが所有する値への借用参照。追加・削除・読込の後は取得し直す必要がある。
// UI側から破棄せず、フレームをまたいで保持しない。ImGuiには依存しない。
struct EditableObjectView {
	WorldObjectId id = kInvalidWorldObjectId;

	const char* name = "";

	TransformData* transform = nullptr;

	Vector4* color = nullptr;

	bool* isLightingEnabled = nullptr;

	float* shininess = nullptr;

	std::string* model = nullptr;

	bool* isRootNodeMatrixUsed = nullptr;

	std::string* editableName = nullptr;

	bool* isActive = nullptr;
	bool* hasMeshRenderer = nullptr;

	WorldObjectType type = WorldObjectType::Model;

	float* lightIntensity = nullptr;
	float* lightRadius = nullptr;
	float* lightDecay = nullptr;

	ParticleSettings* particleSettings = nullptr;

	WorldObjectId parentId = kInvalidWorldObjectId; // 変更はSetObjectParentを通して検証する。

	CameraSettings* camera = nullptr;

	bool isMainCamera = false;

	std::vector<MaterialSettings>* materials = nullptr;

	AnimationSettings* animation = nullptr;

	RigidBodySettings* body = nullptr; // 実行中の速度への参照は公開しない。

	ColliderSettings* collider = nullptr; // 借用参照。構造変更後は取得し直す。

	MotionSettings* motion = nullptr; // 借用参照。フレームをまたいで保持しない。

	LightSettings* light = nullptr; // Light固有設定。構造変更後は取得し直す。
};

