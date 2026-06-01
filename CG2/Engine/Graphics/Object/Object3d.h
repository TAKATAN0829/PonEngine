#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// 自作
#include "EngineStructs.h"
#include "Mesh.h"
#include "Material.h"
#include "Camera.h"
#include "Transform.h"

class Object3d {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(MeshType meshType);

	//=============================================================================================//
	// 更新処理

	void Update(const Matrix4x4& viewProjectionMatrix);

	//=============================================================================================//
	// 描画処理

	void Draw();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Transform設定

	void SetTransform(const TransformData& transform);

	void SetParent(Transform* parent);

	//=============================================================================================//
	// Transform取得

	TransformData GetTransform();

	Transform* GetTransformAddress();

	//=============================================================================================//
	// 色設定

	void SetColor(const Vector4& color);

	//=============================================================================================//
	// Texture番号設定

	void SetTextureIndex(uint32_t textureIndex);

private:

	//=============================================================================================//
	// Mesh

	Mesh* mesh_ = nullptr;

	//=============================================================================================//
	// Transform

	Transform transform_;

	//=============================================================================================//
	// WVPResource

	Microsoft::WRL::ComPtr<ID3D12Resource> wvpResource_ = nullptr;

	//=============================================================================================//
	// WVPData

	TransformationMatrix* wvpData_ = nullptr;

	//=============================================================================================//
	// Material

	Material* material_ = nullptr;
};