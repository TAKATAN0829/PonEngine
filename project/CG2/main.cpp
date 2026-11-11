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

	engine.Run();

	// GPU検証エラーを終了コードへ反映するため、解放前に状態を取得する。
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
