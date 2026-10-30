#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

class DirectXCommon;
class SrvManager;

// Sceneの描画結果をImGuiへ渡す。前フレームのGPU完了後にだけサイズを変更する。
class SceneRenderTarget {
public:
    void Initialize(DirectXCommon* graphics, SrvManager* descriptors);
    void Resize(uint32_t width, uint32_t height);
    void Begin();
    void End();
    void Finalize();
    uint64_t GetTextureHandle() const;
private:
    DirectXCommon* graphics_ = nullptr;
    SrvManager* descriptors_ = nullptr;
    uint32_t descriptorIndex_ = 0;
    uint32_t width_ = 0;
    uint32_t height_ = 0;
    bool initialized_ = false;
    Microsoft::WRL::ComPtr<ID3D12Resource> color_;
    Microsoft::WRL::ComPtr<ID3D12Resource> depth_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> renderHeap_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthHeap_;
};
