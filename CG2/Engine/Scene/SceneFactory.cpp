#include "SceneFactory.h"

// 自作
#include "TitleScene.h"
#include "GameScene.h"

//=============================================================================================//
// Scene生成

IScene* SceneFactory::CreateScene (const std::string& sceneName) {

	if (sceneName == "Title") {

		return new TitleScene ();
	}

	if (sceneName == "Game") {

		return new GameScene ();
	}

	return nullptr;
}