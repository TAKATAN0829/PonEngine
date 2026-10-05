#include "ResourceManager.h"

// LogSystem
#include "LogSystem.h"

// C++
#include <cassert>
#include <string>
#include <vector>
#include "../../../../externals/DirectXTex/d3dx12.h"

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

	LogSystem::Log (std::string (resourceName) + " is Generated!!\n");

	return resource;
}

//=================================================================================================//
// TextureResourceを作る

Microsoft::WRL::ComPtr<ID3D12Resource>
ResourceManager::CreateTextureResource (ID3D12Device* device, const DirectX::TexMetadata& metadata) {

	//=============================================================================================//
	// Resource設定

	D3D12_RESOURCE_DESC resourceDesc{};

	resourceDesc.Width = UINT (metadata.width);
	resourceDesc.Height = UINT (metadata.height);
	resourceDesc.MipLevels = UINT16 (metadata.mipLevels);
	resourceDesc.DepthOrArraySize = UINT16 (metadata.arraySize);
	resourceDesc.Format = metadata.format;
	resourceDesc.SampleDesc.Count = 1;
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION (metadata.dimension);

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

//=================================================================================================//
// TextureResourceにデータを転送

[[nodiscard]]
Microsoft::WRL::ComPtr<ID3D12Resource>
ResourceManager::UploadTextureData (
	const Microsoft::WRL::ComPtr<ID3D12Resource>& texture,
	const DirectX::ScratchImage& mipImages,
	ID3D12Device* device,
	ID3D12GraphicsCommandList* commandList) {

	//=============================================================================================//
	// SubResource作成

	std::vector<D3D12_SUBRESOURCE_DATA> subresources;

	DirectX::PrepareUpload (
		device,
		mipImages.GetImages (),
		mipImages.GetImageCount (),
		mipImages.GetMetadata (),
		subresources);

	//=============================================================================================//
	// IntermediateResourceのサイズ取得

	uint64_t intermediateSize = GetRequiredIntermediateSize (
		texture.Get (),
		0,
		UINT (subresources.size ()));

	//=============================================================================================//
	// IntermediateResource生成

	Microsoft::WRL::ComPtr<ID3D12Resource> intermediateResource = CreateBufferResource (
		device,
		intermediateSize,
		"intermediateResource");

	//=============================================================================================//
	// Textureに転送

	UpdateSubresources (
		commandList,
		texture.Get (),
		intermediateResource.Get (),
		0,
		0,
		UINT (subresources.size ()),
		subresources.data ());

	//=============================================================================================//
	// ResourceBarrier設定

	D3D12_RESOURCE_BARRIER barrier{};

	barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;

	barrier.Flags =	D3D12_RESOURCE_BARRIER_FLAG_NONE;

	barrier.Transition.pResource = texture.Get ();

	barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;

	barrier.Transition.StateAfter =	D3D12_RESOURCE_STATE_GENERIC_READ;

	//=============================================================================================//
	// ResourceBarrierを張る

	commandList->ResourceBarrier (1, &barrier);

	return intermediateResource;
}