#include <Windows.h>
#include <cassert>
#include <string_view>

#include "Engine.h"
#include "LogSystem.h"
#include "D3DResourceLeakChecker.h"
#include "GameScene.h"
#include "TitleScene.h"

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

	const std::string_view arguments(commandLine);

	// 起動引数で最初の場面を選ぶ。通常の起動ではゲーム画面を開く。
	if (arguments.find("--title") != std::string_view::npos) {
		engine.Initialize(new TitleScene());
	} else {
		engine.Initialize(new GameScene());
	}

	// 試験でも本番と同じGameSceneの生成・初期化を通し、古いオブジェクトの混在を検出する。
	const bool isStartupTest = arguments.find("--smoke-test") != std::string_view::npos;

	if (isStartupTest) {
		engine.Run(90);
	} else {
		engine.Run();
	}

	// 描画機能を解放する前に、起動試験の終了コードに使うエラー状態を取得する。
	const bool hasGraphicsErrors = engine.HasGraphicsErrors();

	engine.Finalize ();

	//=============================================================================================//
	// LogSystem終了

	LogSystem::Finalize ();

	//=============================================================================================//
	// COM終了

	CoUninitialize ();

	if (hasGraphicsErrors) {
		return 1;
	}

	return 0;
}
