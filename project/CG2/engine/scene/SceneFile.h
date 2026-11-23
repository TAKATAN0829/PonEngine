#pragma once

#include <filesystem>
#include <string>
#include <ostream>

#include "ModelCollection.h"

/// <summary>
/// シーンの情報を保存・読み込みする
/// </summary>
class SceneFile {
public:
	void Save(const std::string& fileName, const std::string& sceneName, ModelCollection& models, std::string& error);

	void Load(const std::string& fileName, const std::string& sceneName, ModelCollection& models, std::string& error);

private:
	// モデルを書き込む処理
	void WriteModel(std::ostream& file, const SceneModel& model);
};
