#include "Material.h"

// C++
#include <cassert>

// 自作
#include "DirectXCommon.h"
#include "SrvManager.h"
#include "ResourceManager.h"

//=============================================================================================//
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

//=============================================================================================//
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

//=============================================================================================//
// 色

const Vector4& Material::GetColor() const {
	// 描画に使う色を取得する。
	return materialData_->color;
}

void Material::SetColor(const Vector4& color) {
	// 描画に使う色を更新する。

	materialData_->color = color;
}

//=============================================================================================//
// Texture番号設定

void Material::SetTextureIndex(uint32_t textureIndex) {
	// 使用するテクスチャの番号を更新する。

	textureIndex_ = textureIndex;
}

uint32_t Material::GetTextureIndex() const {
	// 使用するテクスチャの番号を取得する。
	return textureIndex_;
}

bool Material::IsLightingEnabled() const {
	// ライティングの有効状態を取得する。
	if (materialData_->enableLighting != 0) {
		return true;
	}

	return false;
}

float Material::GetShininess() const {
	// Materialの光沢度を取得する。
	return materialData_->shininess;
}

//=============================================================================================//
// Lighting有効設定

void Material::SetEnableLighting(bool isLightingEnabled) {
	// ライティングの有効状態を更新する。

	materialData_->enableLighting =	isLightingEnabled;
}

//=============================================================================================//
// UVTransform設定

void Material::SetUVTransform (const Matrix4x4& uvTransform) {
	// MaterialのUV変換行列をGPU用データへ反映する。

	materialData_->uvTransform = uvTransform;
}

//=============================================================================================//
// 光沢度設定

void Material::SetShininess(float shininess) {
	// Materialの光沢度を更新する。
	materialData_->shininess = shininess;
}
