#pragma once

// DirectX・WRL
#include <d3d12.h>
#include <wrl.h>

// C++
#include <cstdint>

class DirectXCommon;
class SrvManager;

// Sceneの描画結果をImGuiへ渡す。サイズ変更と破棄は前フレームのGPU完了後に行う。
class SceneRenderTarget {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* graphics, SrvManager* descriptors);

	//=============================================================================================//
	// メンバ関数 / 描画先の切替

	// Scene描画をBeginとEndで挟み、元のバックバッファへ戻す。
	void Begin();
	void End();

	//=============================================================================================//
	// メンバ関数 / リサイズ

	void Resize(uint32_t width, uint32_t height);

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Getter / Scene画像

	uint64_t GetTextureHandle() const;

private:

	//=============================================================================================//
	// メンバ変数 / 外部機能

	DirectXCommon* graphics_ = nullptr; // Engineが所有する描画機能。
	SrvManager* descriptors_ = nullptr; // SRVの採番・解放を借用する。

	//=============================================================================================//
	// メンバ変数 / SRV・サイズ

	uint32_t descriptorIndex_ = 0; // リサイズ中も同じ番号を維持する。
	uint32_t width_ = 0;
	uint32_t height_ = 0;
	bool initialized_ = false; // SRVを解放すべき状態かを表す。

	//=============================================================================================//
	// メンバ変数 / 描画リソース

	Microsoft::WRL::ComPtr<ID3D12Resource> color_;
	Microsoft::WRL::ComPtr<ID3D12Resource> depth_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> renderHeap_;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> depthHeap_;
};
