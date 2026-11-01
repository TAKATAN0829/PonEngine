#pragma once
#include "IEditableScene.h"
#include "LightStructs.h"

// UI・シーン・描画で同じライト分類と上限を使う。GPUリソースは所有しない。
namespace SceneLighting {
	bool IsLight(SceneObjectType type);
	uint32_t GetLimit(SceneObjectType type);
	const char* GetName(SceneObjectType type);
	// 親子のワールド変換の+Zを方向にする。ゼロ拡縮ではfalseを返す。
	bool GetDirection(const Matrix4x4& world, Vector3& direction);
	SceneLightCollection Collect(const IEditableScene& scene);
	// 旧GameSceneの固定ライト3個を、未使用IDで追加する。失敗時は配列を変更しない。
	bool AddLegacyDefaults(std::vector<SceneObjectData>& objects, std::string& error, SceneObjectId minimumId = 1);
}
