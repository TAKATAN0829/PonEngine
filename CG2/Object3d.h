#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// 自作
#include "EngineStructs.h"
#include "ResourceManager.h"
#include "GraphicsSystem.h"
#include "Mesh.h"

class Object3d {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (ID3D12Device* device, ResourceManager* resourceManager);

	//=============================================================================================//
	// 更新処理

	void Update (const TransformData& cameraTransform, int32_t clientWidth, int32_t clientHeight);

	//=============================================================================================//
	// 描画処理

	void Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem);

	//=============================================================================================//
	// 終了処理

	void Finalize ();

	//=============================================================================================//
	// Transform設定

	void SetTransform (const TransformData& transform);

	//=============================================================================================//
	// Transform取得

	TransformData GetTransform ();

	//=============================================================================================//
	// 色設定

	void SetColor (const Vector4& color);

	//=============================================================================================//
	// Texture番号設定

	void SetTextureIndex (uint32_t textureIndex);

private:

	//=============================================================================================//
	// Mesh

	Mesh* mesh_ = nullptr;

	//=============================================================================================//
	// Transform

	TransformData transform_{};

	//=============================================================================================//
	// MaterialResource

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;

	//=============================================================================================//
	// MaterialData

	Material* materialData_ = nullptr;

	//=============================================================================================//
	// WVPResource

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_ = nullptr;

	//=============================================================================================//
	// WVPData

	TransformationMatrix* wvpData_ = nullptr;

	//=============================================================================================//
	// DirectionalLightResource

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_ = nullptr;

	//=============================================================================================//
	// DirectionalLightData

	DirectionalLight* directionalLightData_ = nullptr;

	//=============================================================================================//
	// Texture番号

	uint32_t textureIndex_ = 0;
};