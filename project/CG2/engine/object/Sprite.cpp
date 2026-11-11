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

//=============================================================================================//
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

//=============================================================================================//
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

//=============================================================================================//
// 描画処理


void Sprite::Draw () {

	ID3D12GraphicsCommandList* commandList = spriteCommon_->GetDxCommon ()->GetCommandList ();

	//=============================================================================================//
	// Material設定

	material_->Bind ();

	//=============================================================================================//
	// 座標変換行列設定

	// このスプライトの位置や大きさを反映した変換を、頂点シェーダーへ渡す。
	commandList->SetGraphicsRootConstantBufferView (1, transformationMatrixResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Mesh描画

	spriteCommon_->GetMesh ()->Draw ();
}

//=============================================================================================//
// 終了処理


void Sprite::Finalize () {
	// Sprite専用のMaterialを解放する。

	delete material_;
}

//=============================================================================================//
// Position

const Vector2& Sprite::GetPosition() const {
	// Spriteの表示位置を取得する。
	return position_;
}

void Sprite::SetPosition(const Vector2& position) {
	// Spriteの表示位置を更新する。
	position_ = position;
}

//=============================================================================================//
// Rotation

float Sprite::GetRotation() const {
	// Spriteの回転角を取得する。
	return rotation_;
}

void Sprite::SetRotation(float rotation) {
	// Spriteの回転角を更新する。
	rotation_ = rotation;
}

//=============================================================================================//
// Size

const Vector2& Sprite::GetSize() const {
	// Spriteの表示サイズを取得する。
	return size_;
}

void Sprite::SetSize(const Vector2& size) {
	// Spriteの表示サイズを更新する。
	size_ = size;
}

//=============================================================================================//
// AnchorPoint

const Vector2& Sprite::GetAnchorPoint() const {
	// 描画の基準位置を取得する。
	return anchorPoint_;
}

void Sprite::SetAnchorPoint(const Vector2& anchorPoint) {
	// 描画の基準位置を更新する。
	anchorPoint_ = anchorPoint;
}

//=============================================================================================//
// Flip

bool Sprite::IsFlippedHorizontally() const {
	// 画像の左右反転の状態を取得する。
	return isFlipX_;
}

void Sprite::SetIsFlipX(bool isFlipX) {
	// 画像の左右反転の状態を更新する。
	isFlipX_ = isFlipX;
}

bool Sprite::IsFlippedVertically() const {
	// 画像の上下反転の状態を取得する。
	return isFlipY_;
}

void Sprite::SetIsFlipY(bool isFlipY) {
	// 画像の上下反転の状態を更新する。
	isFlipY_ = isFlipY;
}

//=============================================================================================//
// TextureLeftTop

const Vector2& Sprite::GetTextureLeftTop() const {
	// 画像の切出し開始位置を取得する。
	return textureLeftTop_;
}

void Sprite::SetTextureLeftTop(const Vector2& textureLeftTop) {
	// 画像の切出し開始位置を更新する。
	textureLeftTop_ = textureLeftTop;
}

//=============================================================================================//
// TextureSize

const Vector2& Sprite::GetTextureSize() const {
	// 画像の切出し範囲を取得する。
	return textureSize_;
}

void Sprite::SetTextureSize(const Vector2& textureSize) {
	// 画像の切出し範囲を更新する。
	textureSize_ = textureSize;
}

//=============================================================================================//
// Color

const Vector4& Sprite::GetColor() const {
	// 描画に使う色を取得する。
	return material_->GetColor();
}

void Sprite::SetColor(const Vector4& color) {
	// 描画に使う色を更新する。
	material_->SetColor(color);
}

//=============================================================================================//
// Texture設定


void Sprite::SetTexture (const std::string& filePath) {
	// 画像のSRVを設定し、必要なら表示サイズを画像へ合わせる。

	textureFilePath_ = filePath;

	material_->SetTextureIndex (TextureManager::GetInstance ()->Load (textureFilePath_));

	AdjustTextureSize ();
}

//=============================================================================================//
// 座標変換行列データ作成


void Sprite::CreateTransformationMatrixData () {
	// 変換行列のGPUバッファを確保し、CPUからの書込先を取得する。

	transformationMatrixResource_ = ResourceManager::CreateBufferResource (spriteCommon_->GetDxCommon ()->GetDevice (), sizeof (TransformationMatrix), "spriteTransformationMatrixResource");

	transformationMatrixResource_->Map (0, nullptr, reinterpret_cast<void**>(&transformationMatrixData_));

	transformationMatrixData_->WVP = MathUtility::MakeIdentity4x4 ();

	transformationMatrixData_->World = MathUtility::MakeIdentity4x4 ();
}

//=============================================================================================//
// テクスチャサイズをイメージに合わせる


void Sprite::AdjustTextureSize () {
	// 読み込んだ画像の寸法を、Spriteの切出し範囲へ合わせる。

	const DirectX::TexMetadata& metadata = TextureManager::GetInstance ()->GetMetaData (textureFilePath_);

	textureImageSize_.x = static_cast<float>(metadata.width);

	textureImageSize_.y = static_cast<float>(metadata.height);

	textureLeftTop_ = { 0.f,0.f };

	textureSize_ = textureImageSize_;

	size_ = textureImageSize_;
}

//=============================================================================================//
// WorldMatrix作成


Matrix4x4 Sprite::MakeWorldMatrix () const {
	// Spriteの基準位置と寸法を反映したワールド行列を作る。

	Matrix4x4 anchorMatrix = MathUtility::MakeTranslateMatrix ({ -anchorPoint_.x,-anchorPoint_.y,0.f });

	Matrix4x4 affineMatrix = MathUtility::MakeAffineMatrix (
		{ size_.x,size_.y,1.f },
		{ 0.f,0.f,rotation_ },
		{ position_.x,position_.y,0.f });

	return MathUtility::Multiply (anchorMatrix, affineMatrix);
}

//=============================================================================================//
// UVTransform作成


Matrix4x4 Sprite::MakeUVTransform () const {
	// 画像の切出し・反転・UV変換を一つの行列へまとめる。

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
