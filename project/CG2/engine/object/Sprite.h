#pragma once

// DirectX
#include <d3d12.h>

// WRL
#include <wrl.h>

// C++
#include <cstdint>

// 自作
#include "EngineStructs.h"

class SpriteCommon;

class Sprite {
public:

	//=============================================================================================//
	// 頂点データ

	struct VertexData {
		Vector4 position;
		Vector2 texcoord;
		Vector3 normal;
	};

	//=============================================================================================//
	// マテリアル

	struct Material {
		Vector4 color;
		int32_t enableLighting;
		float padding[3];
		Matrix4x4 uvTransform;
	};

	//=============================================================================================//
	// 座標変換行列

	struct TransformationMatrix {
		Matrix4x4 WVP;
		Matrix4x4 World;
	};

public:

	//=============================================================================================//
	// 初期化処理

	void Initialize(SpriteCommon* spriteCommon);

	//=============================================================================================//
	// 更新処理

	void Update();

	//=============================================================================================//
	// 描画処理

	void Draw();

	//=============================================================================================//
	// Position

	const Vector2& GetPosition() const { return position_; }

	void SetPosition(const Vector2& position) { this->position_ = position; }

	//=============================================================================================//
	// Rotation

	const Vector2& GetPosition ()const { return position_; }

	void SetPosition (const Vector2& position) { this->position_ = position; }

	//=============================================================================================//
	// Size

	const Vector2& GetSize() const { return size_; }

	void SetSize(const Vector2& size) { this->size_ = size; }

	//=============================================================================================//
	// Rotate設定

	float GetRotation ()const { return rotation_; }

	void SetRotation (float rotation) { this->rotation_ = rotation; }

	//=============================================================================================//
	// Texture番号設定

	const Vector4& GetColor() const { return materialData_->color; }

	void SetColor(const Vector4& color) { materialData_->color = color; }

	//=============================================================================================//
	// Texture番号

	const Vector4& GetColor ()const { return color_; }

	void SetColor (const Vector4& color) { this->color_ = color; }

	//=============================================================================================//
	// Lighting有効設定

	void SetEnableLighting(bool enableLighting) { materialData_->enableLighting = enableLighting; }

private:

	//=============================================================================================//
	// 頂点データ作成

	void CreateVertexData();

	//=============================================================================================//
	// マテリアルデータ作成

	void CreateMaterialData();

	//=============================================================================================//
	// 座標変換行列データ作成

	void CreateTransformationMatrixData();

private:

	//=============================================================================================//
	// SpriteCommon

	SpriteCommon* spriteCommon_ = nullptr;

	//=============================================================================================//
	// Position

	Vector2 position_ = { 0.f,0.f };

	//=============================================================================================//
	// Rotation

	float rotation_ = 0.0f;

	//=============================================================================================//
	// Size

	Vector2 size_ = { 100.f,100.f };

	//=============================================================================================//
	// Rotation

	float rotation_ = 0.f;

	//=============================================================================================//
	// Color

	Vector4 color_ = { 0.f,0.f,0.f,0.f };

	//=============================================================================================//
	// バッファリソース

	Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_ = nullptr;

	Microsoft::WRL::ComPtr<ID3D12Resource> transformationMatrixResource_ = nullptr;

	//=============================================================================================//
	// バッファリソース内のデータを指すポインタ

	VertexData* vertexData_ = nullptr;

	uint32_t* indexData_ = nullptr;

	Material* materialData_ = nullptr;

	TransformationMatrix* wvpData_ = nullptr;
	
};