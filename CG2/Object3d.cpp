#include "Object3d.h"

//=================================================================================================//
// 初期化処理

void Object3d::Initialize (
	ID3D12Device* device,
	ResourceManager* resourceManager) {

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

	mesh_->InitializeSphere (
		device,
		resourceManager);

	//=============================================================================================//
	// MaterialResource生成

	materialResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (Material),
			"materialResource");

	//=============================================================================================//
	// MaterialDataを書き込む

	materialResource_->Map (
		0,
		nullptr,
		reinterpret_cast<void**>(&materialData_));

	materialData_->color =
	{ 1.0f,1.0f,1.0f,1.0f };

	materialData_->enableLighting =
		true;

	materialData_->uvTransform =
		MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// WVPResource生成

	wvpResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (TransformationMatrix),
			"wvpResource");

	//=============================================================================================//
	// WVPDataを書き込む

	wvpResource_->Map (
		0,
		nullptr,
		reinterpret_cast<void**>(&wvpData_));

	wvpData_->WVP =
		MathUtility::MakeIdentity4x4 ();

	wvpData_->World =
		MathUtility::MakeIdentity4x4 ();

	//=============================================================================================//
	// DirectionalLightResource生成

	directionalLightResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (DirectionalLight),
			"directionalLightResource");

	//=============================================================================================//
	// DirectionalLightDataを書き込む

	directionalLightResource_->Map (
		0,
		nullptr,
		reinterpret_cast<void**>(&directionalLightData_));

	directionalLightData_->color =
	{ 1.0f,1.0f,1.0f,1.0f };

	directionalLightData_->direction =
	{ 0.0f,-1.0f,0.0f };

	directionalLightData_->intensity =
		1.0f;
}

//=================================================================================================//
// 更新処理

void Object3d::Update (
	const TransformData& cameraTransform,
	int32_t clientWidth,
	int32_t clientHeight) {

	//=============================================================================================//
	// WVPMatrix作成

	Matrix4x4 worldMatrix =
		MathUtility::MakeAffineMatrix (
			transform_.scale,
			transform_.rotate,
			transform_.translate);

	Matrix4x4 cameraMatrix =
		MathUtility::MakeAffineMatrix (
			cameraTransform.scale,
			cameraTransform.rotate,
			cameraTransform.translate);

	Matrix4x4 viewMatrix =
		MathUtility::Inverse (
			cameraMatrix);

	Matrix4x4 projectionMatrix =
		MathUtility::MakePerspectiveFovMatrix (
			0.45f,
			float (clientWidth) / float (clientHeight),
			0.1f,
			100.0f);

	Matrix4x4 worldViewProjectionMatrix =
		MathUtility::Multiply (
			worldMatrix,
			MathUtility::Multiply (
				viewMatrix,
				projectionMatrix));

	wvpData_->WVP =
		worldViewProjectionMatrix;

	wvpData_->World =
		worldMatrix;
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
		0, materialResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// WVP設定

	commandList->SetGraphicsRootConstantBufferView (
		1, wvpResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Texture設定

	commandList->SetGraphicsRootDescriptorTable (
		2, graphicsSystem->GetSRVDescriptorHeap ()->GetGPUDescriptorHandle (textureIndex_));

	//=============================================================================================//
	// DirectionalLight設定

	commandList->SetGraphicsRootConstantBufferView (
		3, directionalLightResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// DrawCall

	commandList->DrawIndexedInstanced (mesh_->GetIndexCount (), 1, 0, 0, 0);
}

//=================================================================================================//
// 終了処理

void Object3d::Finalize () {

	//=============================================================================================//
	// Mesh解放

	if (mesh_ != nullptr) {

		delete mesh_;

		mesh_ = nullptr;
	}
}

//=================================================================================================//
// Transform設定

void Object3d::SetTransform (
	const TransformData& transform) {

	transform_ =
		transform;
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