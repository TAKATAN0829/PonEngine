#include <Windows.h>
#include <cassert>

#include "Engine.h"
#include "LogSystem.h"
#include "D3DResourceLeakChecker.h"

//=================================================================================================//
// エントリーポイント

int WINAPI WinMain (
	_In_ HINSTANCE,
	_In_opt_ HINSTANCE,
	_In_ LPSTR,
	_In_ int) {

	//=============================================================================================//
	// リークチェッカー

	D3DResourceLeakChecker leakCheck;

	//=============================================================================================//
	// COM初期化

	HRESULT hr = CoInitializeEx (0, COINIT_MULTITHREADED);

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// LogSystem初期化

	LogSystem::Initialize ();

	//=============================================================================================//
	// Engine

	Engine engine;

	engine.Initialize ();

	engine.Run ();

	engine.Finalize ();

	//=============================================================================================//
	// LogSystem終了

	LogSystem::Finalize ();

	//=============================================================================================//
	// COM終了

	CoUninitialize ();

	return 0;
}