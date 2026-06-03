#pragma once

// C++
#include <string>

// 自作
#include "IScene.h"

class SceneFactory {
public:

	//=============================================================================================//
	// Scene生成

	static IScene* CreateScene (const std::string& sceneName);
};