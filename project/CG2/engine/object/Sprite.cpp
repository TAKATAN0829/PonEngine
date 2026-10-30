#include "Sprite.h"

// C++
#include <cassert>

// 自作
#include "SpriteCommon.h"
#include "Mesh.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "TextureManager.h"
#include "WinApp.h"

//=================================================================================================//
// 初期化処理

void Sprite::Initialize (SpriteCommon* spriteCommon, const std::string& textureFilePath) {

	assert (spriteCommon);

	spriteCommon_ = spriteCommon;

	DirectXCommon* dxCommon = spriteCommon_->GetDxCommon ();

	//=============================================================================================//
	// Material生成

	material_ = new Material ();

	material_->Initialize (dxCommon, spriteCommon_->GetSrvManager ());

	material_->SetEnableLighting (false);

	//=============================================================================================//
	// 座標変換行列データ作成

	CreateTransformationMatrixData ();

	//=============================================================================================//
	// Texture読み込み

	SetTexture (textureFilePath);
}

//=================================================================================================//
// 更新処理

void Sprite::Update () {

	//=============================================================================================//
	// UVTransform反映

	material_->SetUVTransform (MakeUVTransform ());

	//=============================================================================================//
	// ProjectionMatrix作成

	Matrix4x4 projectionMatrix = MathUtility::MakeOrthographicMatrix (
		0.f,
		0.f,
		spriteCommon_->GetViewportWidth(),
		spriteCommon_->GetViewportHeight(),
		0.f,
		100.f);

	//=============================================================================================//
	// WVP作成

	Matrix4x4 worldMatrix = MakeWorldMatrix ();

	transformationMatrixData_->WVP = MathUtility::Multiply (worldMatrix, projectionMatrix);

	transformationMatrixData_->World = worldMatrix;
}

//=================================================================================================//
// 描画処理

void Sprite::Draw () {

	ID3D12GraphicsCommandList* commandList = spriteCommon_->GetDxCommon ()->GetCommandList ();

	//=============================================================================================//
	// Material設定

	material_->Bind ();

	//=============================================================================================//
	// 座標変換行列設定

	commandList->SetGraphicsRootConstantBufferView (1, transformationMatrixResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Mesh描画

	spriteCommon_->GetMesh ()->Draw ();
}

//=================================================================================================//
// 終了処理

void Sprite::Finalize () {

	delete material_;
}

//=================================================================================================//
// Texture設定

void Sprite::SetTexture (const std::string& filePath) {

	textureFilePath_ = filePath;

	material_->SetTextureIndex (TextureManager::GetInstance ()->Load (textureFilePath_));

	AdjustTextureSize ();
}

//=================================================================================================//
// テクスチャサイズをイメージに合わせる

void Sprite::AdjustTextureSize () {

	const DirectX::TexMetadata& metadata = TextureManager::GetInstance ()->GetMetaData (textureFilePath_);

	textureImageSize_.x = static_cast<float>(metadata.width);
	textureImageSize_.y = static_cast<float>(metadata.height);

	textureLeftTop_ = { 0.f,0.f };

	textureSize_ = textureImageSize_;

	size_ = textureImageSize_;
}

//=================================================================================================//
// WorldMatrix作成

Matrix4x4 Sprite::MakeWorldMatrix () const {

	Matrix4x4 anchorMatrix = MathUtility::MakeTranslateMatrix ({ -anchorPoint_.x,-anchorPoint_.y,0.f });

	Matrix4x4 affineMatrix = MathUtility::MakeAffineMatrix (
		{ size_.x,size_.y,1.f },
		{ 0.f,0.f,rotation_ },
		{ position_.x,position_.y,0.f });

	return MathUtility::Multiply (anchorMatrix, affineMatrix);
}

//=================================================================================================//
// UVTransform作成

Matrix4x4 Sprite::MakeUVTransform () const {

	Vector3 scale = {
		textureSize_.x / textureImageSize_.x,
		textureSize_.y / textureImageSize_.y,
		1.f };

	Vector3 translate = {
		textureLeftTop_.x / textureImageSize_.x,
		textureLeftTop_.y / textureImageSize_.y,
		0.f };

	if (isFlipX_) {

		translate.x += scale.x;
		scale.x = -scale.x;
	}

	if (isFlipY_) {

		translate.y += scale.y;
		scale.y = -scale.y;
	}

	return MathUtility::Multiply (MathUtility::MakeScaleMatrix (scale), MathUtility::MakeTranslateMatrix (translate));
}

//=================================================================================================//
// 座標変換行列データ作成

void Sprite::CreateTransformationMatrixData () {

	transformationMatrixResource_ = ResourceManager::CreateBufferResource (spriteCommon_->GetDxCommon ()->GetDevice (), sizeof (TransformationMatrix), "spriteTransformationMatrixResource");

	transformationMatrixResource_->Map (0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	transformationMatrixData_->WVP = MathUtility::MakeIdentity4x4 ();

	transformationMatrixData_->World = MathUtility::MakeIdentity4x4 ();
}
