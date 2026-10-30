#include <Windows.h>
#include <cassert>
#include <string_view>

#include "Engine.h"
#include "LogSystem.h"
#include "D3DResourceLeakChecker.h"
#include "GameScene.h"

//=================================================================================================//
// エントリーポイント

int WINAPI WinMain (_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ LPSTR commandLine, _In_ int) {

	//=============================================================================================//
	// リークチェッカー

	D3DResourceLeakChecker leakCheck;

	//=============================================================================================//
	// COM初期化

	const HRESULT comResult = CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	assert(SUCCEEDED(comResult));
	if (FAILED(comResult)) {
		return 1;
	}

	//=============================================================================================//
	// LogSystem初期化

	LogSystem::Initialize ();

	//=============================================================================================//
	// Engine

	Engine engine;

	engine.Initialize (new GameScene ());

	// 試験でも本番と同じGameSceneの生成・初期化を通し、古いオブジェクトの混在を検出する。
	const bool startupTest = std::string_view(commandLine) == "--smoke-test";
	if (startupTest) {
		engine.Run(90);
	} else {
		engine.Run();
	}
	const bool graphicsErrors = engine.HasGraphicsErrors();

	engine.Finalize ();

	//=============================================================================================//
	// LogSystem終了

	LogSystem::Finalize ();

	//=============================================================================================//
	// COM終了

	CoUninitialize ();

	if (graphicsErrors) {
		return 1;
	}
	return 0;
}
