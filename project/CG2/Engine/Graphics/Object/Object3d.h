#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <string.h>

// 自作
#include "EngineStructs.h"
#include "Mesh.h"
#include "Material.h"
#include "Camera.h"
#include "Transform.h"
#include "BlendMode.h"

class Object3d {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(MeshType meshType);

	//=============================================================================================//
	// 初期化処理(OBJモデル)

	void Initialize(const std::string& directoryPath, const std::string& filename);

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

	//=============================================================================================//
	// Lighting設定

	void SetEnableLighting (bool enableLighting);

	//=============================================================================================//
	// UVTransform設定

	void SetUVTransform (const Matrix4x4& uvTransform);

	//=============================================================================================//
	// DirectionalLight設定

	void SetDirectionalLight (
		const Vector4& color,
		const Vector3& direction,
		float intensity);

	//=============================================================================================//
	// Texture設定

	void SetTexture (const std::string& textureName);

	//=============================================================================================//
	// BlendMode設定

	void SetBlendMode (BlendMode blendMode);

	//=============================================================================================//
	// BlendMode取得

	BlendMode GetBlendMode () const;

private:

	//=============================================================================================//
	// Material・WVPの初期化

	void InitializeResources();

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

	//=============================================================================================//
	// BlendMode

	BlendMode blendMode_ = kBlendModeNormal;
};