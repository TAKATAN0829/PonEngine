#pragma once

#include <filesystem>
#include <string>
#include <ostream>

#include "EngineStructs.h"

/// <summary>
/// シーンの情報を保存・読み込みする
/// </summary>
class SceneFile {
public:
	void Save(const std::string& fileName, const std::string& sceneName, const std::string& modelName, const TransformData& transform, std::string& error);

	void Load(const std::string& fileName, const std::string& sceneName, const std::string& modelName, TransformData& transform, std::string& error);

private:
	// モデルを書き込む処理
	void WriteModel(std::ostream& file, const std::string& modelNName, const TransformData& transform);
};
