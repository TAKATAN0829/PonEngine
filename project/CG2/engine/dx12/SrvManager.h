#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>
#include <queue>

class DirectXCommon;

class SrvManager {
public:

	//=============================================================================================//
	// 最大SRV数

	static const uint32_t kMaxSRVCount = 512;

	//=============================================================================================//
	// 初期化処理

	void Initialize (DirectXCommon* dxCommon);

	//=============================================================================================//
	// 描画前処理

	void PreDraw ();

	//=============================================================================================//
	// 空きSRV確保

	uint32_t Allocate ();

	//=============================================================================================//
	// SRV解放

	void Free (uint32_t srvIndex);

	//=============================================================================================//
	// 上限チェック

	bool IsFull () const { return freeIndices_.empty () && useIndex_ >= kMaxSRVCount; }

	//=============================================================================================//
	// SRV生成(Texture2D)

	void CreateSRVforTexture2D (
		uint32_t srvIndex,
		ID3D12Resource* pResource,
		DXGI_FORMAT format,
		UINT mipLevels);

	//=============================================================================================//
	// SRV生成(StructuredBuffer)

	void CreateSRVforStructuredBuffer (
		uint32_t srvIndex,
		ID3D12Resource* pResource,
		UINT numElements,
		UINT structureByteStride);

	//=============================================================================================//
	// DescriptorTable設定

	void SetGraphicsRootDescriptorTable (UINT rootParameterIndex, uint32_t srvIndex);

	//=============================================================================================//
	// Handle取得

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle (uint32_t srvIndex);

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle (uint32_t srvIndex);

	//=============================================================================================//
	// CPUHandleから番号取得

	uint32_t GetIndex (D3D12_CPU_DESCRIPTOR_HANDLE handle);

	//=============================================================================================//
	// DescriptorHeap取得

	ID3D12DescriptorHeap* GetDescriptorHeap () const { return descriptorHeap_.Get (); }

private:

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// DescriptorSize

	uint32_t descriptorSize_ = 0;

	//=============================================================================================//
	// DescriptorHeap

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_ = nullptr;

	//=============================================================================================//
	// 次に使用するSRV番号

	uint32_t useIndex_ = 0;

	//=============================================================================================//
	// 解放されたSRV番号

	std::queue<uint32_t> freeIndices_;
};
