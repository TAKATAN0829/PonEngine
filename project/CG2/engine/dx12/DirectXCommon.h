#pragma once

// Windows
#include <Windows.h>

// C++
#include <cstdint>
#include <array>
#include <chrono>

// DirectX
#include <d3d12.h>
#include <dxgi1_6.h>

// WRL
#include <wrl.h>

class WinApp;

class DirectXCommon {
public:

	//=============================================================================================//
	// BackBuffer数

	static const uint32_t kBufferCount = 2;

public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(WinApp* winApp);

	//=============================================================================================//
	// 描画前処理

	void PreDraw();
	void BindBackBuffer();
	bool HasValidationErrors() const;

	//=============================================================================================//
	// 描画後処理

	void PostDraw();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Getter

	ID3D12Device* GetDevice() { return device_.Get(); }

	ID3D12GraphicsCommandList* GetCommandList() { return commandList_.Get(); }

	ID3D12CommandQueue* GetCommandQueue() { return commandQueue_.Get(); }
	// PostDraw完了後・次のPreDraw前だけ参照可能。所有権は移さない。
	ID3D12Resource* GetPresentedBuffer() const { return swapChainResources_[backBufferIndex_].Get(); }

private:
	//=============================================================================================//
	// メンバ関数 / GPU実行時間

	// タイムスタンプ用のQueryHeapと読取専用バッファを初期化する。
	// 非対応・確保失敗時は描画を継続し、計測値を未取得として扱う。
	void InitializeGpuTiming();
	// GPU完了済みの結果だけを読み、同じフレームの記録へ渡す。追加待機はしない。
	void ReadGpuTiming();

	void ResizeIfNeeded();
	uint32_t renderWidth_ = 0;
	uint32_t renderHeight_ = 0;

	//=============================================================================================//
	// DebugLayer初期化

	void InitializeDebugLayer();

	//=============================================================================================//
	// Device初期化

	void InitializeDevice();

	//=============================================================================================//
	// Command系初期化

	void InitializeCommand();

	//=============================================================================================//
	// SwapChain初期化

	void InitializeSwapChain();

	//=============================================================================================//
	// RTV初期化

	void InitializeRenderTargetView();

	//=============================================================================================//
	// Fence初期化

	void InitializeFence();

	//=============================================================================================//
	// DSV初期化

	void InitializeDepthStencilView();

	//=============================================================================================//
	// Viewport矩形初期化

	void InitializeViewport();

	//=============================================================================================//
	// ScissorRect初期化

	void InitializeScissorRect();

	//=============================================================================================//
	// FPS固定初期化

	void InitializeFixFPS();

	//=============================================================================================//
	// FPS固定更新

	void UpdateFixFPS();

private:

	//=============================================================================================//
	// WinApp

	WinApp* winApp_ = nullptr;

	//=============================================================================================//
	// 記録時間(FPS固定用)

	std::chrono::steady_clock::time_point reference_;

	//=============================================================================================//
	// BackBufferIndex

	UINT backBufferIndex_ = 0;

	//=============================================================================================//
	// Fence

	uint64_t fenceValue_ = 0;

	HANDLE fenceEvent_ = nullptr;

	//=============================================================================================//
	// Viewport

	D3D12_VIEWPORT viewport_{};

	//=============================================================================================//
	// ScissorRect

	D3D12_RECT scissorRect_{};

	//=============================================================================================//
	// Factory

	Microsoft::WRL::ComPtr<IDXGIFactory7> dxgiFactory_;

	//=============================================================================================//
	// Adapter

	Microsoft::WRL::ComPtr<IDXGIAdapter4> useAdapter_;

	//=============================================================================================//
	// Device

	Microsoft::WRL::ComPtr<ID3D12Device> device_;

	//=============================================================================================//
	// CommandQueue

	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue_;

	//=============================================================================================//
	// CommandAllocator

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator_;

	//=============================================================================================//
	// CommandList

	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList_;

	//=============================================================================================//
	// SwapChain

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain_;

	//=============================================================================================//
	// BackBuffer

	std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kBufferCount> swapChainResources_;

	//=============================================================================================//
	// RTVHeap

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvDescriptorHeap_;

	//=============================================================================================//
	// RTVHandle

	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles_[kBufferCount]{};

	//=============================================================================================//
	// Fence

	Microsoft::WRL::ComPtr<ID3D12Fence> fence_;

	//=============================================================================================//
	// DSVHeap

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvDescriptorHeap_;

	//=============================================================================================//
	// DSVHandle

	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle_{};

	//=============================================================================================//
	// DepthStencilResource

	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencilResource_;

	//=============================================================================================//
	// メンバ変数 / GPUタイムスタンプ

	Microsoft::WRL::ComPtr<ID3D12QueryHeap> timestampHeap_; // フレームの開始・終了の2点。
	Microsoft::WRL::ComPtr<ID3D12Resource> timestampReadback_; // フェンス完了後にCPUから読む。
	uint64_t timestampFrequency_ = 0; // コマンドキューのticks/秒。0は計測無効。
};
