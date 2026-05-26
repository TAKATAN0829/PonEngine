#include "ResourceManager.h"

// LogSystem
#include "LogSystem.h"

// C++
#include <cassert>
#include <string>

//=================================================================================================//
// BufferResourceを作る

Microsoft::WRL::ComPtr<ID3D12Resource>
ResourceManager::CreateBufferResource (
	ID3D12Device* device,
	size_t sizeInBytes,
	const char* resourceName) {

	//=============================================================================================//
	// UploadHeap設定

	D3D12_HEAP_PROPERTIES uploadHeapProperties{};

	uploadHeapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	//=============================================================================================//
	// Resource設定

	D3D12_RESOURCE_DESC resourceDesc{};

	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resourceDesc.Width = sizeInBytes;
	resourceDesc.Height = 1;
	resourceDesc.DepthOrArraySize = 1;
	resourceDesc.MipLevels = 1;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//=============================================================================================//
	// Resource生成

	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;

	HRESULT hr = device->CreateCommittedResource (
		&uploadHeapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&resource));

	assert (SUCCEEDED (hr));

	LogSystem::Log (
		std::string (resourceName) + " is Generated!!\n");

	return resource;
}

//=================================================================================================//
// TextureResourceを作る

Microsoft::WRL::ComPtr<ID3D12Resource>
ResourceManager::CreateTextureResource (
	ID3D12Device* device,
	const DirectX::TexMetadata& metadata) {

	//=============================================================================================//
	// Resource設定

	D3D12_RESOURCE_DESC resourceDesc{};

	resourceDesc.Width = UINT (metadata.width);
	resourceDesc.Height = UINT (metadata.height);
	resourceDesc.MipLevels = UINT16 (metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16 (metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension =
		D3D12_RESOURCE_DIMENSION (metadata.dimension);

	//=============================================================================================//
	// Heap設定

	D3D12_HEAP_PROPERTIES heapProperties{};

	heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;

	//=============================================================================================//
	// Resource生成

	Microsoft::WRL::ComPtr<ID3D12Resource> resource = nullptr;

	HRESULT hr = device->CreateCommittedResource (
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS (&resource));

	assert (SUCCEEDED (hr));

	return resource;
}