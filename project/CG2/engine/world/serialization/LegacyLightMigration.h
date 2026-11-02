#pragma once
#include "objects/WorldObjectData.h"

// 形式1〜12に保存されなかった旧固定ライトを、通常のオブジェクトへ移行する。
// 検証後にまとめて置換し、失敗時は呼出元の配列とIDを変更しない。
namespace LegacyLightMigration {
	bool AddLegacyDefaults(std::vector<WorldObjectData>& objects, std::string& error, WorldObjectId minimumId = 1);
}
