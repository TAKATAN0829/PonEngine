#include "DirectXCommon.h"

// C++
#include <cassert>

// Library
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")

// 自作
#include "GraphicsSystem.h"

//=============================================================================================//
// 初期化処理

void DirectXCommon::Initialize (
	HWND hwnd,
	int32_t clientWidth,
	int32_t clientHeight) {

	// WindowSize保存
	clientWidth_ = clientWidth;
	clientHeight_ = clientHeight;

	// DebugLayer初期化
	InitializeDebugLayer ();

	// Device初期化
	InitializeDevice ();

	// Command系初期化
	InitializeCommand ();

	// SwapChain初期化
	InitializeSwapChain (hwnd);

	// RTV初期化
	InitializeRenderTargetView ();

	// DSV初期化
	InitializeDepthStencilView ();

	// Fence初期化
	InitializeFence ();

	// Viewport設定
	viewport_.Width = static_cast<float>(clientWidth_);
	viewport_.Height = static_cast<float>(clientHeight_);
	viewport_.TopLeftX = 0.f;
	viewport_.TopLeftY = 0.f;
	viewport_.MinDepth = 0.f;
	viewport_.MaxDepth = 1.f;

	// ScissorRect設定
	scissorRect_.left = 0;
	scissorRect_.right = clientWidth_;
	scissorRect_.top = 0;
	scissorRect_.bottom = clientHeight_;
}

//=============================================================================================//
// DebugLayer初期化

void DirectXCommon::InitializeDebugLayer () {

#ifdef _DEBUG

	Microsoft::WRL::ComPtr<ID3D12Debug1> debugController = nullptr;

	if (SUCCEEDED (D3D12GetDebugInterface (IID_PPV_ARGS (&debugController)))) {

		// DebugLayerを有効化
		debugController->EnableDebugLayer ();

		// GPUValidation有効化
		debugController->SetEnableGPUBasedValidation (TRUE);
	}

#endif
}

//=============================================================================================//
// Device初期化

void DirectXCommon::InitializeDevice () {

	// DXGIFactory生成
	HRESULT hr = CreateDXGIFactory (
		IID_PPV_ARGS (&dxgiFactory_));

	assert (SUCCEEDED (hr));

	// Adapter検索
	for (
		UINT i = 0;
		dxgiFactory_->EnumAdapterByGpuPreference (
			i,
			DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
			IID_PPV_ARGS (&useAdapter_))
		!= DXGI_ERROR_NOT_FOUND;
		++i) {

		DXGI_ADAPTER_DESC3 adapterDesc{};

		hr = useAdapter_->GetDesc3 (&adapterDesc);

		assert (SUCCEEDED (hr));

		// SoftwareAdapter除外
		if (!(adapterDesc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE)) {
			break;
		}

		useAdapter_ = nullptr;
	}

	assert (useAdapter_ != nullptr);

	// 使用FeatureLevel
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};

	// Device生成
	for (size_t i = 0; i < _countof (featureLevels); ++i) {

		hr = D3D12CreateDevice (
			useAdapter_.Get (),
			featureLevels[i],
			IID_PPV_ARGS (&device_));

		if (SUCCEEDED (hr)) {
			break;
		}
	}

	assert (device_ != nullptr);
}

//=============================================================================================//
// Command系初期化

void DirectXCommon::InitializeCommand () {

	HRESULT hr;

	//=============================================================================================//
	// CommandQueue

	D3D12_COMMAND_QUEUE_DESC commandQueueDesc{};

	hr = device_->CreateCommandQueue (
		&commandQueueDesc,
		IID_PPV_ARGS (&commandQueue_));

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// CommandAllocator

	hr = device_->CreateCommandAllocator (
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS (&commandAllocator_));

	assert (SUCCEEDED (hr));

	//=============================================================================================//
	// CommandList

	hr = device_->CreateCommandList (
		0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		commandAllocator_.Get (),
		nullptr,
		IID_PPV_ARGS (&commandList_));

	assert (SUCCEEDED (hr));
}

//=============================================================================================//
// SwapChain初期化

void DirectXCommon::InitializeSwapChain (HWND hwnd) {

	// SwapChain設定
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};

	swapChainDesc.Width = clientWidth_;
	swapChainDesc.Height = clientHeight_;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

	Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain1 = nullptr;

	// SwapChain生成
	HRESULT hr = dxgiFactory_->CreateSwapChainForHwnd (
		commandQueue_.Get (),
		hwnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain1);

	assert (SUCCEEDED (hr));

	// IDXGISwapChain4へ変換
	hr = swapChain1.As (&swapChain_);

	assert (SUCCEEDED (hr));

	// BackBuffer取得
	hr = swapChain_->GetBuffer (
		0,
		IID_PPV_ARGS (&swapChainResources_[0]));

	assert (SUCCEEDED (hr));

	hr = swapChain_->GetBuffer (
		1,
		IID_PPV_ARGS (&swapChainResources_[1]));

	assert (SUCCEEDED (hr));
}

//=============================================================================================//
// RTV初期化

void DirectXCommon::InitializeRenderTargetView () {

	// RTVHeap設定
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc{};

	rtvDescriptorHeapDesc.Type =
		D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	rtvDescriptorHeapDesc.NumDescriptors = 2;

	rtvDescriptorHeapDesc.Flags =
		D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// RTVHeap生成
	HRESULT hr = device_->CreateDescriptorHeap (
		&rtvDescriptorHeapDesc,
		IID_PPV_ARGS (&rtvDescriptorHeap_));

	assert (SUCCEEDED (hr));

	// DescriptorSize取得
	uint32_t descriptorSizeRTV =
		device_->GetDescriptorHandleIncrementSize (
			D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// RTV設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};

	rtvDesc.Format =
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;

	rtvDesc.ViewDimension =
		D3D12_RTV_DIMENSION_TEXTURE2D;

	// RTVHandle取得
	rtvHandles_[0] =
		rtvDescriptorHeap_->
		GetCPUDescriptorHandleForHeapStart ();

	rtvHandles_[1] = rtvHandles_[0];

	rtvHandles_[1].ptr += descriptorSizeRTV;

	// RTV生成
	device_->CreateRenderTargetView (
		swapChainResources_[0].Get (),
		&rtvDesc,
		rtvHandles_[0]);

	device_->CreateRenderTargetView (
		swapChainResources_[1].Get (),
		&rtvDesc,
		rtvHandles_[1]);
}

//=============================================================================================//
// Fence初期化

void DirectXCommon::InitializeFence () {

	// Fence生成
	HRESULT hr = device_->CreateFence (
		fenceValue_,
		D3D12_FENCE_FLAG_NONE,
		IID_PPV_ARGS (&fence_));

	assert (SUCCEEDED (hr));

	// FenceEvent生成
	fenceEvent_ =
		CreateEvent (
			NULL,
			FALSE,
			FALSE,
			NULL);

	assert (fenceEvent_ != nullptr);
}

//=============================================================================================//
// フレーム開始

void DirectXCommon::BeginFrame () {

	// BackBufferIndex取得
	backBufferIndex_ =
		swapChain_->GetCurrentBackBufferIndex ();

	// Barrier設定
	D3D12_RESOURCE_BARRIER barrier{};

	barrier.Type =
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

	barrier.Flags =
		D3D12_RESOURCE_BARRIER_FLAG_NONE;

	barrier.Transition.pResource =
		swapChainResources_[backBufferIndex_].Get ();

	barrier.Transition.StateBefore =
		D3D12_RESOURCE_STATE_PRESENT;

	barrier.Transition.StateAfter =
		D3D12_RESOURCE_STATE_RENDER_TARGET;

	// Barrier張る
	commandList_->ResourceBarrier (1, &barrier);

	// Viewport設定
	commandList_->RSSetViewports (
		1,
		&viewport_);

	// ScissorRect設定
	commandList_->RSSetScissorRects (
		1,
		&scissorRect_);

	// RenderTarget設定
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle =
		dsvHeap_.GetCPUDescriptorHandle (0);

	commandList_->OMSetRenderTargets (
		1,
		&rtvHandles_[backBufferIndex_],
		false,
		&dsvHandle);

	// ClearColor
	float clearColor[] = {
		0.1f,
		0.25f,
		0.5f,
		1.0f
	};

	// RenderTargetClear
	commandList_->ClearRenderTargetView (
		rtvHandles_[backBufferIndex_],
		clearColor,
		0,
		nullptr);

	// DepthStencilClear
	commandList_->ClearDepthStencilView (
		dsvHandle,
		D3D12_CLEAR_FLAG_DEPTH,
		1.0f,
		0,
		0,
		nullptr);
}

//=============================================================================================//
// フレーム終了

void DirectXCommon::EndFrame () {

	// Barrier設定
	D3D12_RESOURCE_BARRIER barrier{};

	barrier.Type =
		D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

	barrier.Flags =
		D3D12_RESOURCE_BARRIER_FLAG_NONE;

	barrier.Transition.pResource =
		swapChainResources_[backBufferIndex_].Get ();

	barrier.Transition.StateBefore =
		D3D12_RESOURCE_STATE_RENDER_TARGET;

	barrier.Transition.StateAfter =
		D3D12_RESOURCE_STATE_PRESENT;

	// Barrier張る
	commandList_->ResourceBarrier (1, &barrier);

	// CommandList閉じる
	HRESULT hr = commandList_->Close ();

	assert (SUCCEEDED (hr));

	// CommandList配列
	ID3D12CommandList* commandLists[] = {
		commandList_.Get ()
	};

	// Command実行
	commandQueue_->ExecuteCommandLists (
		1,
		commandLists);

	// SwapChainPresent
	swapChain_->Present (1, 0);

	// FenceValue加算
	fenceValue_++;

	// Signal送信
	commandQueue_->Signal (
		fence_.Get (),
		fenceValue_);

	// GPU待機
	if (fence_->GetCompletedValue () < fenceValue_) {

		fence_->SetEventOnCompletion (
			fenceValue_,
			fenceEvent_);

		WaitForSingleObject (
			fenceEvent_,
			INFINITE);
	}

	// CommandAllocatorReset
	hr = commandAllocator_->Reset ();

	assert (SUCCEEDED (hr));

	// CommandListReset
	hr = commandList_->Reset (
		commandAllocator_.Get (),
		nullptr);

	assert (SUCCEEDED (hr));
}

//=============================================================================================//
// 終了処理

void DirectXCommon::Finalize () {

	// FenceEvent解放
	if (fenceEvent_ != nullptr) {

		CloseHandle (fenceEvent_);

		fenceEvent_ = nullptr;
	}
}

//=============================================================================================//
// DSV初期化

void DirectXCommon::InitializeDepthStencilView () {

	// DSVHeap生成
	dsvHeap_.Initialize (D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

	// DepthStencilTexture設定
	D3D12_RESOURCE_DESC resourceDesc{};

	resourceDesc.Width = clientWidth_;
	resourceDesc.Height = clientHeight_;
	resourceDesc.MipLevels = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	// Heap設定
	D3D12_HEAP_PROPERTIES heapProperties{};

	heapProperties.Type =
		D3D12_HEAP_TYPE_DEFAULT;

	// ClearValue設定
	D3D12_CLEAR_VALUE depthClearValue{};

	depthClearValue.DepthStencil.Depth = 1.0f;
	depthClearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// DepthStencilResource生成
	HRESULT hr = device_->CreateCommittedResource (
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS (&depthStencilResource_));

	assert (SUCCEEDED (hr));

	// DSV設定
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc{};

	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;

	// DSV生成
	device_->CreateDepthStencilView (depthStencilResource_.Get (), &dsvDesc, dsvHeap_.GetCPUDescriptorHandle (0));
}