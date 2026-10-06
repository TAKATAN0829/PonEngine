#pragma once

// 自作
#include "WinApp.h"
#include "DirectXCommon.h"
#include "Input.h"
#include "AudioSystem.h"
#include "ShaderCompiler.h"
#include "Object3dRenderer.h"
#include "SpriteRenderer.h"
#include "ParticleRenderer.h"
#include "ImGuiSystem.h"
#include "SceneManager.h"

class Engine {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize ();

	//=============================================================================================//
	// 実行処理

	void Run ();

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

	Object3dRenderer* object3dRenderer_ = nullptr;

	SpriteRenderer* spriteRenderer_ = nullptr;

	ParticleRenderer* particleRenderer_ = nullptr;

	//=============================================================================================//
	// ImGuiSystem

	ImGuiSystem* imGuiSystem_ = nullptr;

	//=============================================================================================//
	// SceneManager

	SceneManager* sceneManager_ = nullptr;
};
