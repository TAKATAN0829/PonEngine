#pragma once

// 自作
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "Input.h"
#include "AudioSystem.h"
#include "ShaderCompiler.h"
#include "Object3dCommon.h"
#include "SpriteCommon.h"
#include "ParticleRenderer.h"
#include "ImGuiSystem.h"
#include "SceneManager.h"
#ifdef USE_IMGUI
#include "../../editor/Editor.h"
#include "SceneRenderTarget.h"
#endif

class Engine {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (IScene* firstScene);

	//=============================================================================================//
	// 実行処理

	// frameLimitが0なら通常実行。起動試験では指定フレーム数だけ描画して終了する。
	void Run (uint32_t frameLimit = 0);
	bool HasGraphicsErrors() const { return dxCommon_->HasValidationErrors(); }

	//=============================================================================================//
	// 終了処理

	void Finalize ();

private:

	//=============================================================================================//
	// Window

	WinApp* winApp_ = nullptr;

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// SrvManager

	SrvManager* srvManager_ = nullptr;

	//=============================================================================================//
	// Input

	Input* input_ = nullptr;

	//=============================================================================================//
	// AudioSystem

	AudioSystem* audioSystem_ = nullptr;

	//=============================================================================================//
	// ShaderCompiler

	ShaderCompiler* shaderCompiler_ = nullptr;

	//=============================================================================================//
	// Renderer

	Object3dCommon* object3dCommon_ = nullptr;

	//=============================================================================================//
	// SpriteCommon

	SpriteCommon* spriteCommon_ = nullptr;

	ParticleRenderer* particleRenderer_ = nullptr;

	//=============================================================================================//
	// ImGuiSystem

	ImGuiSystem* imGuiSystem_ = nullptr;

	//=============================================================================================//
	// SceneManager

	SceneManager* sceneManager_ = nullptr;

#ifdef USE_IMGUI
	Editor editor_;
	SceneRenderTarget sceneTarget_;
#endif
};
