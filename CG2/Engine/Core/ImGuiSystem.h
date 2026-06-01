#pragma once

// Windows
#include <Windows.h>

// DirectX
#include <d3d12.h>

// 自作
#include "DescriptorHeapManager.h"

class ImGuiSystem {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(
		HWND hwnd,
		ID3D12Device* device,
		DescriptorHeapManager* srvDescriptorHeap,
		uint32_t bufferCount,
		uint32_t descriptorIndex);

	//=============================================================================================//
	// 開始処理

	void Begin();

	//=============================================================================================//
	// 描画処理

	void Draw(ID3D12GraphicsCommandList* commandList);

	//=============================================================================================//
	// 終了処理

	void Finalize();

private:

	//=============================================================================================//
	// 初期化済みフラグ

	bool isInitialized_ = false;
};