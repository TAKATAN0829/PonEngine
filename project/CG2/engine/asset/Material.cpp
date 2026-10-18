#include "Material.h"

// C++
#include <cassert>
#include <cmath>
#include <numbers>

// 自作
#include "DirectXCommon.h"
#include "ResourceManager.h"

//=================================================================================================//
// 初期化処理

void Material::Initialize(DirectXCommon* dxCommon) {

	assert(dxCommon);

	dxCommon_ = dxCommon;

	ID3D12Device* device = dxCommon_->GetDevice();

	//=============================================================================================//
	// MaterialResource生成

	materialResource_ = ResourceManager::CreateBufferResource(device, sizeof(MaterialData), "materialResource");

	//=============================================================================================//
	// MaterialDataを書き込む

	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };

	materialData_->enableLighting =	true;

	materialData_->uvTransform = MathUtility::MakeIdentity4x4();

	materialData_->shininess = 40.0f;

	//=============================================================================================//
	// DirectionalLightResource生成

	directionalLightResource_ = ResourceManager::CreateBufferResource(device, sizeof(DirectionalLight), "directionalLightResource");

	//=============================================================================================//
	// DirectionalLightDataを書き込む

	directionalLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	directionalLightData_->direction = { 0.0f,-1.0f,0.0f };

	directionalLightData_->intensity = 1.0f;

	//=============================================================================================//
	// PointLightResource生成

	pointLightResource_ = ResourceManager::CreateBufferResource(device, sizeof(PointLight), "pointLightResource");

	//=============================================================================================//
	// PointLightDataを書き込む

	pointLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&pointLightData_));

	pointLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	pointLightData_->position = { 0.0f,2.0f,0.0f };

	pointLightData_->intensity = 1.0f;

	pointLightData_->radius = 10.0f;

	pointLightData_->decay = 1.0f;

	//=============================================================================================//
	// SpotLightResource生成

	spotLightResource_ = ResourceManager::CreateBufferResource(device, sizeof(SpotLight), "spotLightResource");

	//=============================================================================================//
	// SpotLightDataを書き込む

	spotLightResource_->Map(0, nullptr, reinterpret_cast<void**>(&spotLightData_));

	spotLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	spotLightData_->position = { 2.0f,1.25f,0.0f };

	spotLightData_->distance = 7.0f;

	spotLightData_->direction = MathUtility::Normalize({ -1.0f,-1.0f,0.0f });

	spotLightData_->intensity = 4.0f;

	spotLightData_->decay = 2.0f;

	spotLightData_->cosAngle = std::cos(std::numbers::pi_v<float> / 3.0f);

	spotLightData_->cosFalloffStart = 1.0f;
}

//=================================================================================================//
// RootParameter設定

void Material::Bind() {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//=============================================================================================//
	// Material・Texture設定

	BindWithoutLight();

	//=============================================================================================//
	// DirectionalLight設定

	commandList->SetGraphicsRootConstantBufferView(3, directionalLightResource_->GetGPUVirtualAddress());

	//=============================================================================================//
	// PointLight設定

	commandList->SetGraphicsRootConstantBufferView(5, pointLightResource_->GetGPUVirtualAddress());

	//=============================================================================================//
	// SpotLight設定

	commandList->SetGraphicsRootConstantBufferView(6, spotLightResource_->GetGPUVirtualAddress());
}

//=================================================================================================//
// RootParameter設定（DirectionalLightなし、Particle用）

void Material::BindWithoutLight() {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//=============================================================================================//
	// Material設定

	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	//=============================================================================================//
	// Texture設定

	commandList->SetGraphicsRootDescriptorTable(2, dxCommon_->GetSrvHeap()->GetGPUDescriptorHandle(textureIndex_));
}

//=================================================================================================//
// 色設定

void Material::SetColor(const Vector4& color) {

	materialData_->color = color;
}

//=================================================================================================//
// Texture番号設定

void Material::SetTextureIndex(uint32_t textureIndex) {

	textureIndex_ = textureIndex;
}

//=================================================================================================//
// Lighting有効設定

void Material::SetEnableLighting(bool enableLighting) {

	materialData_->enableLighting =	enableLighting;
}

//=============================================================================================//
// UVTransform設定

void Material::SetUVTransform (const Matrix4x4& uvTransform) {

	materialData_->uvTransform = uvTransform;
}

//=============================================================================================//
// DirectionalLight設定

void Material::SetDirectionalLight (const Vector4& color, const Vector3& direction, float intensity) {

	directionalLightData_->color = color;

	directionalLightData_->direction = MathUtility::Normalize (direction);

	directionalLightData_->intensity = intensity;
}

//=============================================================================================//
// PointLight設定

void Material::SetPointLight(const PointLight& pointLight) {

	*pointLightData_ = pointLight;
}

//=============================================================================================//
// SpotLight設定

void Material::SetSpotLight(const SpotLight& spotLight) {

	*spotLightData_ = spotLight;
}