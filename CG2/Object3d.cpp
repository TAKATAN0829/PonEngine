#include "Object3d.h"

//=================================================================================================//
// 初期化処理

void Object3d::Initialize (ID3D12Device* device, ResourceManager* resourceManager, MeshType meshType) {

	//=============================================================================================//
	// Transform初期値

	transform_ = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	//=============================================================================================//
	// Mesh生成

	mesh_ = new Mesh ();

	mesh_->Initialize (device, resourceManager, meshType);

	//=============================================================================================//
	// MaterialResource生成

	materialResource_ =
		resourceManager->CreateBufferResource (device, sizeof (Material), "materialResource");

	materialResource_->Map (0, nullptr, reinterpret_cast<void**>(&materialData_));

	materialData_->color = { 1.0f,1.0f,1.0f,1.0f };

	materialData_->enableLighting = true;

	materialData_->uvTransform = MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// WVPResource生成

	wvpResource_ =
		resourceManager->CreateBufferResource (device, sizeof (TransformationMatrix), "wvpResource");

	wvpResource_->Map (0, nullptr, reinterpret_cast<void**>(&wvpData_));

	wvpData_->WVP = MathUtility::MakeIdentity4x4 ();

	wvpData_->World = MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// DirectionalLightResource生成

	directionalLightResource_ =
		resourceManager->CreateBufferResource (device, sizeof (DirectionalLight), "directionalLightResource");

	directionalLightResource_->Map (0, nullptr, reinterpret_cast<void**>(&directionalLightData_));

	directionalLightData_->color = { 1.0f,1.0f,1.0f,1.0f };

	directionalLightData_->direction = { 0.0f,-1.0f,0.0f };

	directionalLightData_->intensity = 1.0f;
}

//=================================================================================================//
// 更新処理

void Object3d::Update (const Matrix4x4& viewProjectionMatrix) {

	Matrix4x4 worldMatrix =
		MathUtility::MakeAffineMatrix (
			transform_.scale,
			transform_.rotate,
			transform_.translate);

	Matrix4x4 worldViewProjectionMatrix =
		MathUtility::Multiply (
			worldMatrix,
			viewProjectionMatrix);

	wvpData_->WVP = worldViewProjectionMatrix;

	wvpData_->World = worldMatrix;
}

//=================================================================================================//
// 描画処理

void Object3d::Draw (ID3D12GraphicsCommandList* commandList, GraphicsSystem* graphicsSystem) {

	//=============================================================================================//
	// 描画前設定

	graphicsSystem->PreDraw (
		commandList,
		mesh_->GetVertexBufferView (),
		mesh_->GetIndexBufferView ());

	//=============================================================================================//
	// Material設定

	commandList->SetGraphicsRootConstantBufferView (
		0,
		materialResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// WVP設定

	commandList->SetGraphicsRootConstantBufferView (
		1,
		wvpResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Texture設定

	commandList->SetGraphicsRootDescriptorTable (
		2,
		graphicsSystem->GetSRVDescriptorHeap ()->GetGPUDescriptorHandle (textureIndex_));

	//=============================================================================================//
	// DirectionalLight設定

	commandList->SetGraphicsRootConstantBufferView (
		3,
		directionalLightResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Mesh描画

	mesh_->Draw (commandList);
}

//=================================================================================================//
// 終了処理

void Object3d::Finalize () {

	if (mesh_ != nullptr) {

		delete mesh_;

		mesh_ = nullptr;
	}
}

//=================================================================================================//
// Transform設定

void Object3d::SetTransform (const TransformData& transform) {

	transform_ = transform;
}

//=================================================================================================//
// Transform取得

TransformData Object3d::GetTransform () {

	return transform_;
}

//=================================================================================================//
// 色設定

void Object3d::SetColor (const Vector4& color) {

	materialData_->color = color;
}

//=================================================================================================//
// Texture番号設定

void Object3d::SetTextureIndex (uint32_t textureIndex) {

	textureIndex_ = textureIndex;
}