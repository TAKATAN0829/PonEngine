#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>

// 自作
#include "EngineStructs.h"
#include "ResourceManager.h"
#include "GraphicsSystem.h"

class Material {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize();

	//=============================================================================================//
	// RootParameter設定

	void Bind();

	//=============================================================================================//
	// RootParameter設定（DirectionalLightなし、Particle用）

	void BindWithoutLight();

	//=============================================================================================//
	// 色設定

	void SetColor(const Vector4& color);

	//=============================================================================================//
	// Texture番号設定

	void SetTextureIndex(uint32_t textureIndex);

	//=============================================================================================//
	// Lighting有効設定

	void SetEnableLighting(bool enableLighting);

	//=============================================================================================//
	// UVTransform設定

	void SetUVTransform (const Matrix4x4& uvTransform);

	//=============================================================================================//
	// DirectionalLight設定

	void SetDirectionalLight (const Vector4& color,	const Vector3& direction, float intensity);

private:

	//=============================================================================================//
	// MaterialResource

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;

	//=============================================================================================//
	// MaterialData

	MaterialData* materialData_ = nullptr;

	//=============================================================================================//
	// Texture番号

	uint32_t textureIndex_ = 0;

	//=============================================================================================//
	// DirectionalLightResource

	Microsoft::WRL::ComPtr<ID3D12Resource>directionalLightResource_ = nullptr;

	//=============================================================================================//
	// DirectionalLightData

	DirectionalLight* directionalLightData_ = nullptr;
};