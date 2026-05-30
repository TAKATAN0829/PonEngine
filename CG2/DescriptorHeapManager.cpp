#include "DescriptorHeapManager.h"

// C#
#include <cassert>

// 自作
#include "GraphicsSystem.h"

//=============================================================================================//
// 初期化

void DescriptorHeapManager::Initialize(
	D3D12_DESCRIPTOR_HEAP_TYPE heapType,
	uint32_t descriptorCount,
	bool shaderVisible) {

	ID3D12Device* device = GraphicsSystem::GetDevice ();

	// Heap設定
	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	descriptorHeapDesc.Type = heapType;

	descriptorHeapDesc.NumDescriptors =	descriptorCount;

	if (shaderVisible) {

		descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	} else {

		descriptorHeapDesc.Flags =D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	}

	// Heap生成
	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap_));

	assert(SUCCEEDED(hr));

	// DescriptorSize取得
	descriptorSize_ = device->GetDescriptorHandleIncrementSize(heapType);
}

//=============================================================================================//
// CPUHandle取得

D3D12_CPU_DESCRIPTOR_HANDLE
DescriptorHeapManager::GetCPUDescriptorHandle(
	uint32_t index) {

	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_-> GetCPUDescriptorHandleForHeapStart();

	handle.ptr += descriptorSize_ * index;

	return handle;
}

//=============================================================================================//
// GPUHandle取得

D3D12_GPU_DESCRIPTOR_HANDLE
DescriptorHeapManager::GetGPUDescriptorHandle(uint32_t index) {

	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

	handle.ptr += descriptorSize_ * index;

	return handle;
}