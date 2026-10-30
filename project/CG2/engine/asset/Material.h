#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>

// 自作
#include "EngineStructs.h"

class DirectXCommon;
class SrvManager;

class Material {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

	//=============================================================================================//
	// RootParameter設定

	void Bind();

	//=============================================================================================//
	// 色

	const Vector4& GetColor() const { return materialData_->color; }

	void SetColor(const Vector4& color);

	//=============================================================================================//
	// Texture番号設定

	void SetTextureIndex(uint32_t textureIndex);
	uint32_t GetTextureIndex() const { return textureIndex_; }
	bool GetEnableLighting() const { return materialData_->enableLighting != 0; }
	float GetShininess() const { return materialData_->shininess; }

	//=============================================================================================//
	// Lighting有効設定

	void SetEnableLighting(bool enableLighting);

	//=============================================================================================//
	// UVTransform設定

	void SetUVTransform (const Matrix4x4& uvTransform);

	//=============================================================================================//
	// 光沢度設定

	void SetShininess(float shininess) { materialData_->shininess = shininess; }

private:

	//=============================================================================================//
	// マテリアル（GPU転送用）

	struct MaterialForGPU {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
		float shininess;
	};

	//=============================================================================================//
	// DirectXCommon

	DirectXCommon* dxCommon_ = nullptr;

	//=============================================================================================//
	// SrvManager

	SrvManager* srvManager_ = nullptr;

	//=============================================================================================//
	// MaterialResource

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;

	//=============================================================================================//
	// MaterialData

	MaterialForGPU* materialData_ = nullptr;

	//=============================================================================================//
	// Texture番号

	uint32_t textureIndex_ = 0;
};
