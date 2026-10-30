#include "SceneRenderTarget.h"
#include "DirectXCommon.h"
#include "SrvManager.h"
#include <stdexcept>

// 専用のRTV/DSVを用意し、ImGuiへ渡すSRV番号はリサイズしても変えない。
void SceneRenderTarget::Initialize(DirectXCommon* graphics, SrvManager* descriptors) {
    graphics_ = graphics;
    descriptors_ = descriptors;
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.NumDescriptors = 1;
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    if (FAILED(graphics_->GetDevice()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&renderHeap_)))) {
        throw std::runtime_error("Scene RTV heap creation failed");
    }
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    if (FAILED(graphics_->GetDevice()->CreateDescriptorHeap(&heapDescription, IID_PPV_ARGS(&depthHeap_)))) {
        throw std::runtime_error("Scene DSV heap creation failed");
    }
    descriptorIndex_ = descriptors_->Allocate();
    initialized_ = true;
    Resize(800, 500);
}

// 新しいリソースの作成に成功するまでは、現在の描画先を保持する。
void SceneRenderTarget::Resize(uint32_t width, uint32_t height) {
    if (width == 0 || height == 0 || (width == width_ && height == height_)) { return; }
    D3D12_HEAP_PROPERTIES heapProperties{};
    heapProperties.Type = D3D12_HEAP_TYPE_DEFAULT;
    D3D12_RESOURCE_DESC description{};
    description.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    description.Width = width;
    description.Height = height;
    description.DepthOrArraySize = 1;
    description.MipLevels = 1;
    description.SampleDesc.Count = 1;
    description.Format = DXGI_FORMAT_R8G8B8A8_TYPELESS;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
    D3D12_CLEAR_VALUE clearValue{};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    clearValue.Color[0] = 0.055f;
    clearValue.Color[1] = 0.065f;
    clearValue.Color[2] = 0.085f;
    clearValue.Color[3] = 1;
    Microsoft::WRL::ComPtr<ID3D12Resource> nextColor;
    if (FAILED(graphics_->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
        &description, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, &clearValue, IID_PPV_ARGS(&nextColor)))) {
        throw std::runtime_error("Scene color texture creation failed");
    }
    description.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    description.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    clearValue = {};
    clearValue.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    clearValue.DepthStencil.Depth = 1;
    Microsoft::WRL::ComPtr<ID3D12Resource> nextDepth;
    if (FAILED(graphics_->GetDevice()->CreateCommittedResource(&heapProperties, D3D12_HEAP_FLAG_NONE,
        &description, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS(&nextDepth)))) {
        throw std::runtime_error("Scene depth texture creation failed");
    }
    color_ = nextColor;
    depth_ = nextDepth;
    width_ = width;
    height_ = height;
    D3D12_RENDER_TARGET_VIEW_DESC renderView{};
    renderView.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    renderView.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
    graphics_->GetDevice()->CreateRenderTargetView(color_.Get(), &renderView, renderHeap_->GetCPUDescriptorHandleForHeapStart());
    D3D12_DEPTH_STENCIL_VIEW_DESC depthView{};
    depthView.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    depthView.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    graphics_->GetDevice()->CreateDepthStencilView(depth_.Get(), &depthView, depthHeap_->GetCPUDescriptorHandleForHeapStart());
    descriptors_->CreateSRVforTexture2D(descriptorIndex_, color_.Get(), DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, 1);
}

// サンプリング用の状態から描画用へ切り替え、パネルと同じ寸法で描画する。
void SceneRenderTarget::Begin() {
    ID3D12GraphicsCommandList* commands = graphics_->GetCommandList();
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = color_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    commands->ResourceBarrier(1, &barrier);
    const D3D12_CPU_DESCRIPTOR_HANDLE renderHandle = renderHeap_->GetCPUDescriptorHandleForHeapStart();
    const D3D12_CPU_DESCRIPTOR_HANDLE depthHandle = depthHeap_->GetCPUDescriptorHandleForHeapStart();
    commands->OMSetRenderTargets(1, &renderHandle, false, &depthHandle);
    const float clearColor[4] = {0.055f, 0.065f, 0.085f, 1};
    commands->ClearRenderTargetView(renderHandle, clearColor, 0, nullptr);
    commands->ClearDepthStencilView(depthHandle, D3D12_CLEAR_FLAG_DEPTH, 1, 0, 0, nullptr);
    const D3D12_VIEWPORT viewport = {0, 0, static_cast<float>(width_), static_cast<float>(height_), 0, 1};
    const D3D12_RECT scissor = {0, 0, static_cast<LONG>(width_), static_cast<LONG>(height_)};
    commands->RSSetViewports(1, &viewport);
    commands->RSSetScissorRects(1, &scissor);
}

// ImGuiが同じフレームで読み取れるよう、描画結果をサンプリング用へ戻す。
void SceneRenderTarget::End() {
    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = color_.Get();
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    graphics_->GetCommandList()->ResourceBarrier(1, &barrier);
    graphics_->BindBackBuffer();
}

// 共有SRV管理より先に解放し、再起動や終了時に記述子を残さない。
void SceneRenderTarget::Finalize() {
    color_.Reset();
    depth_.Reset();
    if (initialized_) { descriptors_->Free(descriptorIndex_); }
    initialized_ = false;
}

// 描画リソース自体ではなく、ImGuiが利用する共有ヒープ内のハンドルを返す。
uint64_t SceneRenderTarget::GetTextureHandle() const {
    return descriptors_->GetGPUDescriptorHandle(descriptorIndex_).ptr;
}
