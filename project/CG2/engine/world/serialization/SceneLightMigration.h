#pragma once
#include "objects/SceneObjectData.h"

// 形式1〜12に保存されなかった旧固定ライトを、通常のオブジェクトへ移行する。
// 検証後にまとめて置換し、失敗時は呼出元の配列とIDを変更しない。
namespace SceneLightMigration {
	bool AddLegacyDefaults(std::vector<SceneObjectData>& objects, std::string& error, SceneObjectId minimumId = 1);
}
