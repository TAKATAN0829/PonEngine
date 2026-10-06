#pragma once

#include <cstdint>
#include <d3d12.h>
#include <wrl.h>

class DescriptorHeapManager {
public:

	//=============================================================================================//
	// 初期化

	void Initialize (
		ID3D12Device* device,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		uint32_t descriptorCount,
		bool shaderVisible);

	//=============================================================================================//
	// CPUHandle取得

	D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle (uint32_t index);

	//=============================================================================================//
	// GPUHandle取得

	D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle (uint32_t index);

	//=============================================================================================//
	// 空きDescriptor確保

	uint32_t Allocate ();

	//=============================================================================================//
	// Getter

	ID3D12DescriptorHeap* GetDescriptorHeap () { return descriptorHeap_.Get (); }

private:
	//=============================================================================================//
	// Device

	Microsoft::WRL::ComPtr<ID3D12Device> device_;

	//=============================================================================================//
	// DescriptorHeap

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_;

	//=============================================================================================//
	// DescriptorSize

	uint32_t descriptorSize_ = 0;

	//=============================================================================================//
	// Descriptor数

	uint32_t descriptorCount_ = 0;

	//=============================================================================================//
	// 次に確保する番号

	uint32_t nextIndex_ = 0;
};
