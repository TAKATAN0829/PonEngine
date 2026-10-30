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

	//=============================================================================================//
	// 終了処理

	void Finalize ();


	//=============================================================================================//
	// Getter / GPU検証結果

	bool HasGraphicsErrors() const { return dxCommon_->HasValidationErrors(); }

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


	//=============================================================================================//
	// メンバ変数 / Particle描画

	ParticleRenderer* particleRenderer_ = nullptr;

	//=============================================================================================//
	// ImGuiSystem

	ImGuiSystem* imGuiSystem_ = nullptr;

	//=============================================================================================//
	// SceneManager

	SceneManager* sceneManager_ = nullptr;

#ifdef USE_IMGUI

	//=============================================================================================//
	// メンバ変数 / Editor・Scene描画先

	// Editorは編集状態を所有し、SceneRenderTargetは画像用GPUリソースを所有する。
	Editor editor_;
	SceneRenderTarget sceneTarget_;
#endif
};
