#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DescriptorHeapManager {
public:

	//=============================================================================================//
	// 初期化

	void Initialize (D3D12_DESCRIPTOR_HEAP_TYPE heapType, uint32_t descriptorCount, bool shaderVisible);

	//=============================================================================================//
	// CPUHandle取得

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle (uint32_t index);

	//=============================================================================================//
	// GPUHandle取得

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle (uint32_t index);

	//=============================================================================================//
	// Getter

	ID3D12DescriptorHeap* GetDescriptorHeap () {
		return descriptorHeap_.Get ();
	}

private:

	//=============================================================================================//
	// Device

	ID3D12Device* device_ = nullptr;

	//=============================================================================================//
	// DescriptorHeap

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	//=============================================================================================//
	// DescriptorSize

	uint32_t descriptorSize_ = 0;
};