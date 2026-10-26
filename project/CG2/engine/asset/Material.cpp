#include "Material.h"

// C++
#include <cassert>

// 自作
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ResourceManager.h"

//=================================================================================================//
// 初期化処理

void Material::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager) {

	assert(dxCommon);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;

	ID3D12Device* device = dxCommon_->GetDevice();

	//=============================================================================================//
	// MaterialResource生成

	materialResource_ = ResourceManager::CreateBufferResource(device, sizeof(MaterialForGPU), "materialResource");

	//=============================================================================================//
	// MaterialDataを書き込む

	materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };

	materialData_->enableLighting =	true;

	materialData_->uvTransform = MathUtility::MakeIdentity4x4();

	materialData_->shininess = 40.0f;
}

//=================================================================================================//
// RootParameter設定

void Material::Bind() {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//=============================================================================================//
	// Material設定

	commandList->SetGraphicsRootConstantBufferView(0, materialResource_->GetGPUVirtualAddress());

	//=============================================================================================//
	// Texture設定

	srvManager_->SetGraphicsRootDescriptorTable(2, textureIndex_);
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
