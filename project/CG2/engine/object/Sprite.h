#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>
#include <string>

// 自作
#include "EngineStructs.h"
#include "Material.h"

class SpriteCommon;

class Sprite {
public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(SpriteCommon* spriteCommon, const std::string& textureFilePath);

	//=============================================================================================//
	// 更新処理

	void Update();

	//=============================================================================================//
	// 描画処理

	void Draw();

	//=============================================================================================//
	// 終了処理

	void Finalize();

	//=============================================================================================//
	// Position

	const Vector2& GetPosition() const;

	void SetPosition(const Vector2& position);

	//=============================================================================================//
	// Rotation

	float GetRotation() const;

	void SetRotation(float rotation);

	//=============================================================================================//
	// Size

	const Vector2& GetSize() const;

	void SetSize(const Vector2& size);

	//=============================================================================================//
	// AnchorPoint

	const Vector2& GetAnchorPoint() const;

	void SetAnchorPoint(const Vector2& anchorPoint);

	//=============================================================================================//
	// Flip

	bool IsFlippedHorizontally() const;

	void SetIsFlipX(bool isFlipX);

	bool IsFlippedVertically() const;

	void SetIsFlipY(bool isFlipY);

	//=============================================================================================//
	// TextureLeftTop

	const Vector2& GetTextureLeftTop() const;

	void SetTextureLeftTop(const Vector2& textureLeftTop);

	//=============================================================================================//
	// TextureSize

	const Vector2& GetTextureSize() const;

	void SetTextureSize(const Vector2& textureSize);

	//=============================================================================================//
	// Color

	const Vector4& GetColor() const;

	void SetColor(const Vector4& color);

	//=============================================================================================//
	// Texture設定

	void SetTexture(const std::string& filePath);

private:

	//=============================================================================================//
	// 座標変換行列データ作成

	void CreateTransformationMatrixData();

	//=============================================================================================//
	// テクスチャサイズをイメージに合わせる

	void AdjustTextureSize();

	//=============================================================================================//
	// WorldMatrix作成

	Matrix4x4 MakeWorldMatrix() const;

	//=============================================================================================//
	// UVTransform作成

	Matrix4x4 MakeUVTransform() const;

private:

	//=============================================================================================//
	// SpriteCommon

	SpriteCommon* spriteCommon_ = nullptr;

	//=============================================================================================//
	// Material

	Material* material_ = nullptr;

	//=============================================================================================//
	// Position

	Vector2 position_ = { 0.f,0.f };

	//=============================================================================================//
	// Rotation

	float rotation_ = 0.f;

	//=============================================================================================//
	// Size

	Vector2 size_ = { 100.f,100.f };

	//=============================================================================================//
	// AnchorPoint

	Vector2 anchorPoint_ = { 0.f,0.f };

	//=============================================================================================//
	// Flip

	// 左右フリップ
	bool isFlipX_ = false;

	// 上下フリップ
	bool isFlipY_ = false;

	//=============================================================================================//
	// Texture範囲

	// テクスチャ左上座標
	Vector2 textureLeftTop_ = { 0.f,0.f };

	// テクスチャ切り出しサイズ
	Vector2 textureSize_ = { 100.f,100.f };

	//=============================================================================================//
	// Textureファイルパス
	   
	std::string textureFilePath_;

	//=============================================================================================//
	// Texture画像サイズ

	Vector2 textureImageSize_ = { 1.f,1.f };

	//=============================================================================================//
	// 座標変換行列

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_ = nullptr;

	TransformationMatrix* transformationMatrixData_ = nullptr;
};
