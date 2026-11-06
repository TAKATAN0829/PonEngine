#include "SrvManager.h"

// C++
#include <cassert>

// 自作
#include "DirectXCommon.h"

//=============================================================================================//
// 初期化処理


void SrvManager::Initialize(DirectXCommon* dxCommon) {

	assert(dxCommon);

	dxCommon_ = dxCommon;

	ID3D12Device* device = dxCommon_->GetDevice();

	//=============================================================================================//
	// DescriptorHeap生成

	D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc{};

	descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	descriptorHeapDesc.NumDescriptors = kMaxSRVCount;

	descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

	HRESULT hr = device->CreateDescriptorHeap(&descriptorHeapDesc, IID_PPV_ARGS(&descriptorHeap_));

	assert(SUCCEEDED(hr));

	//=============================================================================================//
	// DescriptorSize取得

	descriptorSize_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
}

//=============================================================================================//
// 描画前処理


void SrvManager::PreDraw() {
	// 共有するSRVヒープを描画命令へ設定する。

	ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };

	dxCommon_->GetCommandList()->SetDescriptorHeaps(1, descriptorHeaps);
}

//=============================================================================================//
// 空きSRV確保


uint32_t SrvManager::Allocate() {
	// 再利用できる番号を優先し、空きがなければ次のSRV番号を割り当てる。

	assert(!IsFull());

	// 解放済みの番号を先に再利用し、未使用の枠を消費しない。
	if (!freeIndices_.empty()) {

		uint32_t srvIndex = freeIndices_.front();

		freeIndices_.pop();

		return srvIndex;
	}

	return useIndex_++;
}

//=============================================================================================//
// SRV解放


void SrvManager::Free(uint32_t srvIndex) {
	// 使用済みのSRV番号を、再利用できる一覧へ戻す。

	assert(srvIndex < useIndex_);

	freeIndices_.push(srvIndex);
}

//=============================================================================================//
// 上限チェック

bool SrvManager::IsFull() const {
	// 再利用できる番号と新規割当の上限から、SRVの空きを調べる。
	if (freeIndices_.empty () && useIndex_ >= kMaxSRVCount) {
		return true;
	}

	return false;
}

//=============================================================================================//
// SRV生成(Texture2D)


void SrvManager::CreateSRVforTexture2D(
	uint32_t srvIndex,
	ID3D12Resource* pResource,
	DXGI_FORMAT format,
	UINT mipLevels) {
	// 画像の形式とMip数を設定し、2DテクスチャのSRVを作る。

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	srvDesc.Format = format;

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;

	srvDesc.Texture2D.MipLevels = mipLevels;

	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

//=============================================================================================//
// SRV生成(StructuredBuffer)


void SrvManager::CreateSRVforStructuredBuffer(
	uint32_t srvIndex,
	ID3D12Resource* pResource,
	UINT numElements,
	UINT structureByteStride) {
	// 要素数と要素サイズを設定し、構造化バッファのSRVを作る。

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};

	srvDesc.Format = DXGI_FORMAT_UNKNOWN;

	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;

	srvDesc.Buffer.FirstElement = 0;

	srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

	srvDesc.Buffer.NumElements = numElements;

	srvDesc.Buffer.StructureByteStride = structureByteStride;

	dxCommon_->GetDevice()->CreateShaderResourceView(pResource, &srvDesc, GetCPUDescriptorHandle(srvIndex));
}

//=============================================================================================//
// DescriptorTable設定


void SrvManager::SetGraphicsRootDescriptorTable(UINT rootParameterIndex, uint32_t srvIndex) {
	// 指定したRootParameterへSRVのGPUハンドルを設定する。

	dxCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(rootParameterIndex, GetGPUDescriptorHandle(srvIndex));
}

//=============================================================================================//
// Handle取得


D3D12_CPU_DESCRIPTOR_HANDLE SrvManager::GetCPUDescriptorHandle(uint32_t srvIndex) {
	// 指定番号のCPU用記述子ハンドルを取得する。

	D3D12_CPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	handle.ptr += static_cast<SIZE_T>(descriptorSize_) * srvIndex;

	return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE SrvManager::GetGPUDescriptorHandle(uint32_t srvIndex) {
	// 指定番号のGPU用記述子ハンドルを取得する。

	D3D12_GPU_DESCRIPTOR_HANDLE handle = descriptorHeap_->GetGPUDescriptorHandleForHeapStart();

	handle.ptr += static_cast<UINT64>(descriptorSize_) * srvIndex;

	return handle;
}

//=============================================================================================//
// CPUHandleから番号取得


uint32_t SrvManager::GetIndex(D3D12_CPU_DESCRIPTOR_HANDLE handle) {
	// CPU用記述子ハンドルの位置から、ヒープ内の番号を求める。

	D3D12_CPU_DESCRIPTOR_HANDLE start = descriptorHeap_->GetCPUDescriptorHandleForHeapStart();

	return static_cast<uint32_t>((handle.ptr - start.ptr) / descriptorSize_);
}

//=============================================================================================//
// DescriptorHeap取得

ID3D12DescriptorHeap* SrvManager::GetDescriptorHeap() const {
	// 描画に使う記述子ヒープを取得する。
	return descriptorHeap_.Get ();
}
