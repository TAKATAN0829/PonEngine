#include "Object3d.h"

//=================================================================================================//
// 初期化処理

void Object3d::Initialize(MeshType meshType) {

	ID3D12Device* device = GraphicsSystem::GetDevice();

	ResourceManager* resourceManager =	ResourceManager::GetInstance();

	transform_ = {
		{1.0f,1.0f,1.0f},
		{0.0f,0.0f,0.0f},
		{0.0f,0.0f,0.0f}
	};

	mesh_ = new Mesh();

	mesh_->Initialize(meshType);

	material_ = new Material();

	material_->Initialize();

	wvpResource_ =
		resourceManager->CreateBufferResource(
			device,
			sizeof(TransformationMatrix),
			"wvpResource");

	wvpResource_->Map(
		0,
		nullptr,
		reinterpret_cast<void**> (&wvpData_));

	wvpData_->WVP =	MathUtility::MakeIdentity4x4();

	wvpData_->World = MathUtility::MakeIdentity4x4();
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

void Object3d::Draw () {

	//=============================================================================================//
	// 描画前設定
	ID3D12GraphicsCommandList* commandList = GraphicsSystem::GetCommandList();
	GraphicsSystem* graphicsSystem = GraphicsSystem::GetGraphicsSystem();

	graphicsSystem->PreDraw (
		commandList,
		mesh_->GetVertexBufferView (),
		mesh_->GetIndexBufferView ());

	//=============================================================================================//
	// Material設定

	material_->Bind();

	//=============================================================================================//
	// WVP設定

	commandList->SetGraphicsRootConstantBufferView (
		1,
		wvpResource_->GetGPUVirtualAddress ());

	//=============================================================================================//
	// Mesh描画

	mesh_->Draw ();
}

//=================================================================================================//
// 終了処理

void Object3d::Finalize () {

	if (mesh_ != nullptr) {

		delete mesh_;

		mesh_ = nullptr;
	}

	if (material_ != nullptr) {

		delete material_;

		material_ = nullptr;
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

	material_->SetColor(color);
}

//=================================================================================================//
// Texture番号設定

void Object3d::SetTextureIndex(uint32_t textureIndex) {

	material_->SetTextureIndex(textureIndex);
}