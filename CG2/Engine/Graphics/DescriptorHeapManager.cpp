#include "DescriptorHeapManager.h"

#include <cassert>

//=============================================================================================//
// 初期化

void DescriptorHeapManager::Initialize (
	ID3D12Device* device,
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	uint32_t descriptorCount,
	bool shaderVisible) {

	device_ = device;

	// Heap設定
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	descriptorHeapDesc.Type = heapType;

	descriptorHeapDesc.NumDescriptors =
		descriptorCount;

	if (shaderVisible) {

		descriptorHeapDesc.Flags =
			D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	} else {

		descriptorHeapDesc.Flags =
			D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}

	// Heap生成
	HRESULT hr =
		device_->CreateDescriptorHeap (
			&descriptorHeapDesc,
			IID_PPV_ARGS (&descriptorHeap_));

	assert (SUCCEEDED (hr));

	// DescriptorSize取得
	descriptorSize_ =
		device_->GetDescriptorHandleIncrementSize (
			heapType);
}

//=============================================================================================//
// CPUHandle取得

D3D12_CPU_DESCRIPTOR_HANDLE
DescriptorHeapManager::GetCPUDescriptorHandle (
	uint32_t index) {

	D3D12_CPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap_->
		GetCPUDescriptorHandleForHeapStart ();

	handle.ptr += descriptorSize_ * index;

	return handle;
}

//=============================================================================================//
// GPUHandle取得

D3D12_GPU_DESCRIPTOR_HANDLE
DescriptorHeapManager::GetGPUDescriptorHandle (uint32_t index) {

	D3D12_GPU_DESCRIPTOR_HANDLE handle =
		descriptorHeap_->GetGPUDescriptorHandleForHeapStart ();

	handle.ptr += descriptorSize_ * index;

	return handle;
}