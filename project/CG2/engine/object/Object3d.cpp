#include "Object3d.h"

// C++
#include <cassert>

// 自作
#include "Object3dRenderer.h"
#include "DirectXCommon.h"
#include "ResourceManager.h"
#include "TextureManager.h"


//=================================================================================================//
// 初期化処理

void Object3d::Initialize(Object3dRenderer* renderer, MeshType meshType) {

	assert(renderer);

	renderer_ = renderer;

	mesh_ = new Mesh();

	mesh_->Initialize(renderer_->GetDxCommon(), meshType);

	InitializeResources();
}


//=================================================================================================//
// 初期化処理(OBJモデル)

void Object3d::Initialize(
	Object3dRenderer* renderer,
	const std::string& directoryPath,
	const std::string& filename) {

	assert(renderer);

	renderer_ = renderer;

	mesh_ = new Mesh();

	mesh_->Initialize(
		renderer_->GetDxCommon(),
		directoryPath,
		filename);

	InitializeResources();
}


//=================================================================================================//
// Material・WVPの初期化

void Object3d::InitializeResources() {

	DirectXCommon* dxCommon = renderer_->GetDxCommon();

	material_ = new Material();

	material_->Initialize(dxCommon);

	wvpResource_ = ResourceManager::CreateBufferResource(
		dxCommon->GetDevice(),
		sizeof(TransformationMatrix),
		"wvpResource");

	wvpResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**> (&wvpData_));

	wvpData_->WVP = MathUtility::MakeIdentity4x4();

	wvpData_->World = MathUtility::MakeIdentity4x4();
}


//=================================================================================================//
// 更新処理

void Object3d::Update(const Matrix4x4& viewProjectionMatrix) {

	transform_.UpdateMatrix ();

	Matrix4x4 worldMatrix = transform_.GetWorldMatrix();

	Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply(worldMatrix, viewProjectionMatrix);

	wvpData_->WVP = worldViewProjectionMatrix;

	wvpData_->World = worldMatrix;
}


//=================================================================================================//
// 描画処理

void Object3d::Draw() {

	//=============================================================================================//
	// 描画前設定
	renderer_->PreDraw(blendMode_);

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

void Object3d::Finalize() {
	delete mesh_;
	delete material_;

}


//=================================================================================================//
// Transform設定

void Object3d::SetTransform(const TransformData& transform) {

	transform_.local_ = transform;
}


//=================================================================================================//
// Transform取得

TransformData Object3d::GetTransform() {

	return transform_.local_;
}


//=================================================================================================//
// 色設定

void Object3d::SetColor(const Vector4& color) {

	material_->SetColor(color);
}


//=================================================================================================//
// Lighting設定

void Object3d::SetEnableLighting (bool enableLighting) {

	material_->SetEnableLighting (enableLighting);
}


//=================================================================================================//
// UVTransform設定

void Object3d::SetUVTransform (const Matrix4x4& uvTransform) {

	material_->SetUVTransform (uvTransform);
}


//=================================================================================================//
// DirectionalLight設定

void Object3d::SetDirectionalLight (
	const Vector4& color,
	const Vector3& direction,
	float intensity) {

	material_->SetDirectionalLight (
		color,
		direction,
		intensity);
}


//=================================================================================================//
// Texture番号設定

void Object3d::SetTextureIndex(uint32_t textureIndex) {

	material_->SetTextureIndex(textureIndex);
}


//=============================================================================================//
// Transformアドレス取得

Transform* Object3d::GetTransformAddress() {

	return &transform_;
}


//=============================================================================================//
// 親設定

void Object3d::SetParent(Transform* parent) {

	transform_.SetParent(parent);
}


//=============================================================================================//
// Texture設定

void Object3d::SetTexture (const std::string& filePath) {

	uint32_t textureIndex = TextureManager::GetInstance ()->Load (filePath);

	SetTextureIndex (textureIndex);
}


//=============================================================================================//
// BlendMode設定

void Object3d::SetBlendMode (BlendMode blendMode) {

	blendMode_ = blendMode;
}


//=============================================================================================//
// BlendMode取得

BlendMode Object3d::GetBlendMode () const {

	return blendMode_;
}