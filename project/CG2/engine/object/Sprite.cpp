#include "Sprite.h"

// C++
#include <cassert>

// 自作
#include "SpriteCommon.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "WinApp.h"

//=================================================================================================//
// 初期化処理

void Sprite::Initialize (SpriteCommon* spriteCommon, const std::string& textureFilePath) {

	assert (spriteCommon);

	spriteCommon_ = spriteCommon;

	// 頂点データ作成
	CreateVertexData ();

	// マテリアルデータ作成
	CreateMaterialData ();

	// 座標変換行列データ作成
	CreateTransformationMatrixData ();

	// Texture読み込み
	SetTexture (textureFilePath);

	// テクスチャサイズをイメージに合わせる
	AdjustTextureSize ();
}

//=================================================================================================//
// 更新処理

void Sprite::Update () {

	//=============================================================================================//
	// AnchorPointを反映した頂点座標

	float left = 0.f - anchorPoint_.x;
	float right = 1.f - anchorPoint_.x;
	float top = 0.f - anchorPoint_.y;
	float bottom = 1.f - anchorPoint_.y;

	// 左右反転
	if (isFlipX_) {

		left = -left;
		right = -right;
	}

	// 上下反転
	if (isFlipY_) {

		top = -top;
		bottom = -bottom;
	}

	// 左下
	vertexData_[0].position = { left,bottom,0.f,1.f };

	// 左上
	vertexData_[1].position = { left,top,0.f,1.f };

	// 右下
	vertexData_[2].position = { right,bottom,0.f,1.f };

	// 右上
	vertexData_[3].position = { right,top,0.f,1.f };

	//=============================================================================================//
	// Texture範囲を反映したUV座標

	const DirectX::TexMetadata& metadata = TextureManager::GetInstance ()->GetMetaData (textureIndex_);

	float texLeft = textureLeftTop_.x / static_cast<float>(metadata.width);
	float texRight = (textureLeftTop_.x + textureSize_.x) / static_cast<float>(metadata.width);
	float texTop = textureLeftTop_.y / static_cast<float>(metadata.height);
	float texBottom = (textureLeftTop_.y + textureSize_.y) / static_cast<float>(metadata.height);

	// 左下
	vertexData_[0].texcoord = { texLeft,texBottom };

	// 左上
	vertexData_[1].texcoord = { texLeft,texTop };

	// 右下
	vertexData_[2].texcoord = { texRight,texBottom };

	// 右上
	vertexData_[3].texcoord = { texRight,texTop };

	//=============================================================================================//
	// WorldMatrix作成

	Matrix4x4 worldMatrix = MathUtility::MakeAffineMatrix (
		{ size_.x,size_.y,1.f },
		{ 0.f,0.f,rotation_ },
		{ position_.x,position_.y,0.f });

	//=============================================================================================//
	// ViewMatrix作成

	Matrix4x4 viewMatrix = MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// ProjectionMatrix作成

	Matrix4x4 projectionMatrix = MathUtility::MakeOrthographicMatrix (
		0.f,
		0.f,
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight),
		0.f,
		100.f);

	//=============================================================================================//
	// WVP作成

	transformationMatrixData_->WVP = MathUtility::Multiply (worldMatrix, MathUtility::Multiply (viewMatrix, projectionMatrix));

	transformationMatrixData_->World = worldMatrix;
}

//=================================================================================================//
// 描画処理

void Sprite::Draw () {

	// 共通描画設定はSpriteCommon::DrawSettingCommonで描画前に一度だけ行う

	DirectXCommon* dxCommon = spriteCommon_->GetDxCommon ();

	ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList ();

	//=============================================================================================//
	// VBV・IBV設定

	commandList->IASetVertexBuffers (0, 1, &vertexBufferView_);

	commandList->IASetIndexBuffer (&indexBufferView_);

	//=============================================================================================//
	// Material設定

	commandList->SetGraphicsRootConstantBufferView (0, materialResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// 座標変換行列設定

	commandList->SetGraphicsRootConstantBufferView (1, transformationMatrixResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Texture設定

	commandList->SetGraphicsRootDescriptorTable (2, dxCommon->GetSrvHeap ()->GetGPUDescriptorHandle (textureIndex_));

	//=============================================================================================//
	// DirectionalLight設定

	commandList->SetGraphicsRootConstantBufferView (3, directionalLightResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// 描画

	commandList->DrawIndexedInstanced (6, 1, 0, 0, 0);
}

//=================================================================================================//
// Texture設定

void Sprite::SetTexture (const std::string& filePath) {

	textureIndex_ = TextureManager::GetInstance ()->Load (filePath);
}

//=================================================================================================//
// テクスチャサイズをイメージに合わせる

void Sprite::AdjustTextureSize () {

	// テクスチャメタデータを取得
	const DirectX::TexMetadata& metadata = TextureManager::GetInstance ()->GetMetaData (textureIndex_);

	textureSize_.x = static_cast<float>(metadata.width);
	textureSize_.y = static_cast<float>(metadata.height);

	// 画像サイズをテクスチャサイズに合わせる
	size_ = textureSize_;
}

//=================================================================================================//
// 頂点データ作成

void Sprite::CreateVertexData () {

	ID3D12Device* device = spriteCommon_->GetDxCommon ()->GetDevice ();

	//=============================================================================================//
	// VertexResource・IndexResource生成

	vertexResource_ = ResourceManager::CreateBufferResource (
		device,
		sizeof (VertexData) * 4,
		"spriteVertexResource");

	indexResource_ = ResourceManager::CreateBufferResource (
		device,
		sizeof (uint32_t) * 6,
		"spriteIndexResource");

	//=============================================================================================//
	// VertexBufferView作成

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress ();

	vertexBufferView_.SizeInBytes = sizeof (VertexData) * 4;

	vertexBufferView_.StrideInBytes = sizeof (VertexData);

	//=============================================================================================//
	// IndexBufferView作成

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress ();

	indexBufferView_.SizeInBytes = sizeof (uint32_t) * 6;

	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	//=============================================================================================//
	// VertexDataを書き込む

	vertexResource_->Map (0, nullptr, reinterpret_cast<void**>(&vertexData_));

	// 左下
	vertexData_[0].position = { 0.f,1.f,0.f,1.f };
	vertexData_[0].texcoord = { 0.f,1.f };
	vertexData_[0].normal = { 0.f,0.f,-1.f };

	// 左上
	vertexData_[1].position = { 0.f,0.f,0.f,1.f };
	vertexData_[1].texcoord = { 0.f,0.f };
	vertexData_[1].normal = { 0.f,0.f,-1.f };

	// 右下
	vertexData_[2].position = { 1.f,1.f,0.f,1.f };
	vertexData_[2].texcoord = { 1.f,1.f };
	vertexData_[2].normal = { 0.f,0.f,-1.f };

	// 右上
	vertexData_[3].position = { 1.f,0.f,0.f,1.f };
	vertexData_[3].texcoord = { 1.f,0.f };
	vertexData_[3].normal = { 0.f,0.f,-1.f };

	//=============================================================================================//
	// IndexDataを書き込む

	indexResource_->Map (0, nullptr, reinterpret_cast<void**>(&indexData_));

	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;

	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;
}

//=================================================================================================//
// マテリアルデータ作成

void Sprite::CreateMaterialData () {

	ID3D12Device* device = spriteCommon_->GetDxCommon ()->GetDevice ();

	//=============================================================================================//
	// MaterialResource生成

	materialResource_ = ResourceManager::CreateBufferResource (
		device,
		sizeof (Material),
		"spriteMaterialResource");

	//=============================================================================================//
	// MaterialDataを書き込む

	materialResource_->Map (0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.f,1.f,1.f,1.f };

	materialData_->enableLighting = false;

	materialData_->uvTransform = MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// DirectionalLightResource生成

	directionalLightResource_ = ResourceManager::CreateBufferResource (
		device,
		sizeof (DirectionalLight),
		"spriteDirectionalLightResource");

	directionalLightResource_->Map (0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	directionalLightData_->color = { 1.f,1.f,1.f,1.f };

	directionalLightData_->direction = { 0.f,-1.f,0.f };

	directionalLightData_->intensity = 1.f;
}

//=================================================================================================//
// 座標変換行列データ作成

void Sprite::CreateTransformationMatrixData () {

	//=============================================================================================//
	// TransformationMatrixResource生成

	transformationMatrixResource_ = ResourceManager::CreateBufferResource (
		spriteCommon_->GetDxCommon ()->GetDevice (),
		sizeof (TransformationMatrix),
		"spriteTransformationMatrixResource");

	//=============================================================================================//
	// TransformationMatrixDataを書き込む

	transformationMatrixResource_->Map (0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	transformationMatrixData_->WVP = MathUtility::MakeIdentity4x4 ();

	transformationMatrixData_->World = MathUtility::MakeIdentity4x4 ();
}
