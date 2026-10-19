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
#include "../asset/Model.h"

class Object3dCommon;

class Object3d {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (Object3dCommon* object3dCommon, MeshType meshType);

	//=============================================================================================//
	// 初期化処理(OBJモデル)

	void Initialize (Object3dCommon* object3dCommon, const std::string& directoryPath, const std::string& filename);

	//=============================================================================================//
	// 更新処理

	void Update (const Matrix4x4& viewProjectionMatrix);

	//=============================================================================================//
	// 描画処理

	void Draw ();

	//=============================================================================================//
	// 終了処理

	void Finalize ();

	//=============================================================================================//
	// Transform設定

	void SetTransform (const TransformData& transform);

	void SetParent (Transform* parent);

	//=============================================================================================//
	// Transform取得

	TransformData GetTransform ();

	Transform* GetTransformAddress ();

	//=============================================================================================//
	// 色設定

	void SetColor (const Vector4& color);

	//=============================================================================================//
	// Texture番号設定

	void SetTextureIndex (uint32_t textureIndex);

	//=============================================================================================//
	// Lighting設定

	void SetEnableLighting (bool enableLighting);

	//=============================================================================================//
	// UVTransform設定

	void SetUVTransform (const Matrix4x4& uvTransform);

	//=============================================================================================//
	// 光沢度設定

	void SetShininess (float shininess) { material_->SetShininess (shininess); }

	//=============================================================================================//
	// RootNode行列の適用設定

	void SetUseRootNodeMatrix (bool useRootNodeMatrix) { useRootNodeMatrix_ = useRootNodeMatrix; }

	//=============================================================================================//
	// DirectionalLight設定

	void SetDirectionalLight (const Vector4& color, const Vector3& direction, float intensity);

	//=============================================================================================//
	// PointLight設定

	void SetPointLight (const PointLight& pointLight) { material_->SetPointLight (pointLight); }

	//=============================================================================================//
	// SpotLight設定

	void SetSpotLight (const SpotLight& spotLight) { material_->SetSpotLight (spotLight); }

	//=============================================================================================//
	// Texture設定

	void SetTexture (const std::string& filePath);

	//=============================================================================================//
	// BlendMode設定

	void SetBlendMode (BlendMode blendMode);

	//=============================================================================================//
	// BlendMode取得

	BlendMode GetBlendMode () const;

	// モデル設定
	void SetModel(Model* model) { this->model_ = model; }
private:

	//=============================================================================================//
	// Material・WVPの初期化

	void InitializeResources ();

private:

	//=============================================================================================//
	// Renderer

	Object3dCommon* object3dCommon_ = nullptr;

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

	//=============================================================================================//
	// RootNode行列の適用

	bool useRootNodeMatrix_ = true;

	//Model

	Model* model_ = nullptr;
};