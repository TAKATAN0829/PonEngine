#pragma once

#include <filesystem>
#include <string>
#include <ostream>

#include "EngineStructs.h"

class SceneFile {
public:
	void Save(const std::string& fileName, const std::string& sceneName, const std::string& modelName, const TransformData& transform, std::string& error);

	void Load(const std::string& fileName, const std::string& sceneName, const std::string& modelName, TransformData& transform, std::string& error);

private:
	void WriteModel(std::ostream& file, const std::string& modelNName, const TransformData& transform);
};
