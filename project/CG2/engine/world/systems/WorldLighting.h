#pragma once

#include "IWorldObjects.h"
#include "LightStructs.h"

// 実行中のライトを共通インターフェースから集める。保存・旧形式移行・一覧管理は担当しない。
namespace WorldLighting {
	//=============================================================================================//
	// ライト計算

	// 親子のワールド変換の+Zを方向にする。ゼロ拡縮ではfalseを返す。
	bool GetDirection(const Matrix4x4& world, Vector3& direction);

	WorldLightCollection Collect(const IWorldObjects& scene);
}
