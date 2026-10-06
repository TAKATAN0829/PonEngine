#include "Material.h"

// C++
#include <cassert>

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

	materialResource_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(MaterialData),
		"materialResource");

	//=============================================================================================//
	// MaterialDataを書き込む

	materialResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };

	materialData_->enableLighting =	true;

	materialData_->uvTransform = MathUtility::MakeIdentity4x4();

	//=============================================================================================//
	// DirectionalLightResource生成

	directionalLightResource_ = ResourceManager::CreateBufferResource(
		device,
		sizeof(DirectionalLight),
		"directionalLightResource");

	//=============================================================================================//
	// DirectionalLightDataを書き込む

	directionalLightResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&directionalLightData_));

	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	directionalLightData_->direction = { 0.0f,-1.0f,0.0f };

	directionalLightData_->intensity = 1.0f;
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

void Material::SetDirectionalLight (
	const Vector4& color,
	const Vector3& direction,
	float intensity) {

	directionalLightData_->color = color;

	directionalLightData_->direction = MathUtility::Normalize (direction);

	directionalLightData_->intensity = intensity;
}