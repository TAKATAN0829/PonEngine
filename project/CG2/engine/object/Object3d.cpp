#include "Object3d.h"

// C++
#include <cassert>
#include <cmath>
#include <numbers>

// 自作
#include "Object3dCommon.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "TextureManager.h"


//=================================================================================================//
// 初期化処理

void Object3d::Initialize (Object3dCommon* object3dCommon) {

	assert (object3dCommon);

	object3dCommon_ = object3dCommon;

	CreateTransformationMatrixData ();

	CreateDirectionalLightData ();

	CreatePointLightData ();

	CreateSpotLightData ();
}


//=================================================================================================//
// 座標変換行列データ作成

void Object3d::CreateTransformationMatrixData () {

	wvpResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (TransformationMatrix), "wvpResource");

	wvpResource_->Map (0, nullptr, reinterpret_cast<void**> (&wvpData_));

	wvpData_->WVP = MathUtility::MakeIdentity4x4 ();

	wvpData_->World = MathUtility::MakeIdentity4x4 ();

	wvpData_->WorldInverseTranspose = MathUtility::MakeIdentity4x4 ();
}


//=================================================================================================//
// 平行光源データ作成

void Object3d::CreateDirectionalLightData () {

	directionalLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (DirectionalLight), "directionalLightResource");

	directionalLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&directionalLightData_));

	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	directionalLightData_->direction = { 0.0f,-1.0f,0.0f };

	directionalLightData_->intensity = 1.0f;
}


//=================================================================================================//
// 点光源データ作成

void Object3d::CreatePointLightData () {

	pointLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (PointLight), "pointLightResource");

	pointLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&pointLightData_));

	pointLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	pointLightData_->position = { 0.0f,2.0f,0.0f };

	pointLightData_->intensity = 1.0f;

	pointLightData_->radius = 10.0f;

	pointLightData_->decay = 1.0f;
}


//=================================================================================================//
// スポットライトデータ作成

void Object3d::CreateSpotLightData () {

	spotLightResource_ = ResourceManager::CreateBufferResource (object3dCommon_->GetDxCommon ()->GetDevice (), sizeof (SpotLight), "spotLightResource");

	spotLightResource_->Map (0, nullptr, reinterpret_cast<void**> (&spotLightData_));

	spotLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	spotLightData_->position = { 2.0f,1.25f,0.0f };

	spotLightData_->distance = 7.0f;

	spotLightData_->direction = MathUtility::Normalize ({ -1.0f,-1.0f,0.0f });

	spotLightData_->intensity = 4.0f;

	spotLightData_->decay = 2.0f;

	spotLightData_->cosAngle = std::cos (std::numbers::pi_v<float> / 3.0f);

	spotLightData_->cosFalloffStart = 1.0f;
}


//=================================================================================================//
// 更新処理

void Object3d::Update (const Matrix4x4& viewProjectionMatrix) {

	transform_.UpdateMatrix ();

	Matrix4x4 worldMatrix = transform_.GetWorldMatrix ();

	if (useRootNodeMatrix_) {

		worldMatrix = MathUtility::Multiply (model_->GetRootNode ().localMatrix, worldMatrix);
	}

	Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply (worldMatrix, viewProjectionMatrix);

	wvpData_->WVP = worldViewProjectionMatrix;

	wvpData_->World = worldMatrix;

	wvpData_->WorldInverseTranspose = MathUtility::Transpose (MathUtility::Inverse (worldMatrix));
}


//=================================================================================================//
// 描画処理

void Object3d::Draw () {

	//=============================================================================================//
	// BlendMode設定

	object3dCommon_->SetBlendMode (blendMode_);

	ID3D12GraphicsCommandList* commandList = object3dCommon_->GetDxCommon ()->GetCommandList ();

	//=============================================================================================//
	// WVP設定

	commandList->SetGraphicsRootConstantBufferView (1, wvpResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// ライト設定

	commandList->SetGraphicsRootConstantBufferView (3, directionalLightResource_->GetGPUVirtualAddress ());

	commandList->SetGraphicsRootConstantBufferView (5, pointLightResource_->GetGPUVirtualAddress ());

	commandList->SetGraphicsRootConstantBufferView (6, spotLightResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Model描画

	model_->Draw ();
}


//=================================================================================================//
// Transform設定

void Object3d::SetTransform (const TransformData& transform) {

	transform_.local_ = transform;
}


//=================================================================================================//
// Transform取得

TransformData Object3d::GetTransform () {

	return transform_.local_;
}


//=================================================================================================//
// 色設定

void Object3d::SetColor (const Vector4& color) {

	model_->GetMaterial ()->SetColor (color);
}


//=================================================================================================//
// Lighting設定

void Object3d::SetEnableLighting (bool enableLighting) {

	model_->GetMaterial ()->SetEnableLighting (enableLighting);
}


//=================================================================================================//
// UVTransform設定

void Object3d::SetUVTransform (const Matrix4x4& uvTransform) {

	model_->GetMaterial ()->SetUVTransform (uvTransform);
}


//=================================================================================================//
// DirectionalLight設定

void Object3d::SetDirectionalLight (const Vector4& color, const Vector3& direction, float intensity) {

	directionalLightData_->color = color;

	directionalLightData_->direction = MathUtility::Normalize (direction);

	directionalLightData_->intensity = intensity;
}


//=================================================================================================//
// Texture番号設定

void Object3d::SetTextureIndex (uint32_t textureIndex) {

	model_->GetMaterial ()->SetTextureIndex (textureIndex);
}


//=============================================================================================//
// Transformアドレス取得

Transform* Object3d::GetTransformAddress () {

	return &transform_;
}


//=============================================================================================//
// 親設定

void Object3d::SetParent (Transform* parent) {

	transform_.SetParent (parent);
}


//=============================================================================================//
// Texture設定

void Object3d::SetTexture (const std::string& filePath) {

	uint32_t textureIndex = TextureManager::GetInstance ()->Load (filePath);

	SetTextureIndex (textureIndex);
}


//=============================================================================================//
// BlendMode設定

void Object3d::SetBlendMode (BlendMode blendMode) {

	blendMode_ = blendMode;
}


//=============================================================================================//
// BlendMode取得

BlendMode Object3d::GetBlendMode () const {

	return blendMode_;
}
