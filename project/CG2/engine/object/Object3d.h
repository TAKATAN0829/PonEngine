#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <string>
#include <memory>

// 自作
#include "EngineStructs.h"
#include "LightStructs.h"
#include "Model.h"
#include "Camera.h"
#include "Transform.h"
#include "BlendMode.h"

class Object3dCommon;

class Object3d {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize (Object3dCommon* object3dCommon);

	//=============================================================================================//
	// 更新処理

	void Update (const Matrix4x4& viewProjectionMatrix);

	//=============================================================================================//
	// 描画処理

	void Draw ();

	//=============================================================================================//
	// Model設定

	void SetModel (Model* model);

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

	void SetPointLight (const PointLight& pointLight) { *pointLightData_ = pointLight; }

	//=============================================================================================//
	// SpotLight設定

	void SetSpotLight (const SpotLight& spotLight) { *spotLightData_ = spotLight; }

	//=============================================================================================//
	// Texture設定

	void SetTexture (const std::string& filePath);

	//=============================================================================================//
	// BlendMode設定

	void SetBlendMode (BlendMode blendMode);

	//=============================================================================================//
	// BlendMode取得

	BlendMode GetBlendMode () const;

private:

	//=============================================================================================//
	// 座標変換行列データ作成

	void CreateTransformationMatrixData ();

	//=============================================================================================//
	// 平行光源データ作成

	void CreateDirectionalLightData ();

	//=============================================================================================//
	// 点光源データ作成

	void CreatePointLightData ();

	//=============================================================================================//
	// スポットライトデータ作成

	void CreateSpotLightData ();

private:

	//=============================================================================================//
	// Object3dCommon

	Object3dCommon* object3dCommon_ = nullptr;

	//=============================================================================================//
	// Model

	Model* model_ = nullptr;
	std::unique_ptr<Material> material_; // 同じModelを使う別のオブジェクトへ色や光沢を波及させない。

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
	// 平行光源

	Microsoft::WRL::ComPtr<ID3D12Resource> directionalLightResource_ = nullptr;

	DirectionalLight* directionalLightData_ = nullptr;

	//=============================================================================================//
	// 点光源

	Microsoft::WRL::ComPtr<ID3D12Resource> pointLightResource_ = nullptr;

	PointLight* pointLightData_ = nullptr;

	//=============================================================================================//
	// スポットライト

	Microsoft::WRL::ComPtr<ID3D12Resource> spotLightResource_ = nullptr;

	SpotLight* spotLightData_ = nullptr;

	//=============================================================================================//
	// BlendMode

	BlendMode blendMode_ = kBlendModeNormal;

	//=============================================================================================//
	// RootNode行列の適用

	bool useRootNodeMatrix_ = true;
};
