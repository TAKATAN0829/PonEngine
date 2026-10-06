#include "Sprite.h"

// C++
#include <cassert>

// 自作
#include "SpriteRenderer.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "WinApp.h"

//=================================================================================================//
// 初期化処理

void Sprite::Initialize(SpriteRenderer* renderer) {

	assert(renderer);

	renderer_ = renderer;

	DirectXCommon* dxCommon = renderer_->GetDxCommon();

	//=============================================================================================//
	// Mesh生成

	mesh_ = new Mesh();

	mesh_->Initialize(dxCommon, MeshType::kSprite);

	//=============================================================================================//
	// Material生成

	material_ = new Material();

	material_->Initialize(dxCommon);

	//=============================================================================================//
	// WVPResource生成

	wvpResource_ = ResourceManager::CreateBufferResource(
		dxCommon->GetDevice(),
		sizeof(TransformationMatrix),
		"spriteWVPResource");

	//=============================================================================================//
	// WVPDataを書き込む

	wvpResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**>(&wvpData_));

	wvpData_->WVP = MathUtility::MakeIdentity4x4();

	wvpData_->World = MathUtility::MakeIdentity4x4();
}

//=================================================================================================//
// 更新処理

void Sprite::Update() {

	//=============================================================================================//
	// WorldMatrix作成

	Matrix4x4 worldMatrix = MathUtility::MakeAffineMatrix(
		{ size_.x * 0.5f,size_.y * 0.5f,1.0f },
		{ 0.0f,0.0f,0.0f },
		{ position_.x, position_.y,0.0f });

	//=============================================================================================//
	// OrthographicMatrix作成

	Matrix4x4 orthographicMatrix = MathUtility::MakeOrthographicMatrix(
		0.0f,
		0.0f,
		static_cast<float>(WinApp::kClientWidth),
		static_cast<float>(WinApp::kClientHeight),
		0.0f,
		100.0f);

	//=============================================================================================//
	// WVP作成

	Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply(worldMatrix, orthographicMatrix);

	wvpData_->WVP = worldViewProjectionMatrix;

	wvpData_->World = worldMatrix;
}

//=================================================================================================//
// 描画処理

void Sprite::Draw() {

	//=============================================================================================//
	// 描画前設定

	renderer_->PreDraw();

	ID3D12GraphicsCommandList* commandList = renderer_->GetDxCommon()->GetCommandList();

	//=============================================================================================//
	// Material設定

	material_->Bind();

	//=============================================================================================//
	// WVP設定

	commandList->SetGraphicsRootConstantBufferView(1, wvpResource_->GetGPUVirtualAddress());

	//=============================================================================================//
	// Mesh描画

	mesh_->Draw();
}

//=================================================================================================//
// 終了処理

void Sprite::Finalize() {

	//=============================================================================================//
	// Mesh解放

	if (mesh_ != nullptr) {

		delete mesh_;

		mesh_ = nullptr;
	}

	//=============================================================================================//
	// Material解放

	if (material_ != nullptr) {

		delete material_;

		material_ = nullptr;
	}
}

//=================================================================================================//
// Position設定

void Sprite::SetPosition(const Vector2& position) {

	position_ = position;
}

//=================================================================================================//
// Size設定

void Sprite::SetSize(const Vector2& size) {

	size_ = size;
}

//=================================================================================================//
// Texture番号設定

void Sprite::SetTextureIndex(uint32_t textureIndex) {

	material_->SetTextureIndex(textureIndex);
}

//=================================================================================================//
// 色設定

void Sprite::SetColor(const Vector4& color) {

	material_->SetColor(color);
}

//=================================================================================================//
// Lighting有効設定

void Sprite::SetEnableLighting(bool enableLighting) {

	material_->SetEnableLighting(enableLighting);
}