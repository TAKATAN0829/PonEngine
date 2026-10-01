#include "Particle.h"

// C++
#include <cassert>

// 自作
#include "GraphicsSystem.h"
#include "ResourceManager.h"
#include "MathUtility.h"

//=================================================================================================//
// 初期化処理

void Particle::Initialize () {

	ID3D12Device* device = GraphicsSystem::GetDevice ();

	ResourceManager* resourceManager = ResourceManager::GetInstance ();

	DescriptorHeapManager* srvDescriptorHeap = GraphicsSystem::GetGraphicsSystem ()->GetSRVDescriptorHeap ();

	//=============================================================================================//
	// Mesh生成

	mesh_ = new Mesh ();

	mesh_->Initialize (MeshType::kPlane);

	//=============================================================================================//
	// Material生成

	material_ = new Material ();

	material_->Initialize ();

	//=============================================================================================//
	// Instancing用のTransformationMatrixリソースを作る

	instancingResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (TransformationMatrix) * kNumInstance,
			"instancingResource");

	//=============================================================================================//
	// 書き込むためのアドレスを取得

	instancingResource_->Map (0, nullptr, reinterpret_cast<void**>(&instancingData_));

	// 単位行列を書き込んでおく
	for (uint32_t index = 0; index < kNumInstance; ++index) {

		instancingData_[index].WVP = MathUtility::MakeIdentity4x4 ();

		instancingData_[index].World = MathUtility::MakeIdentity4x4 ();
	}

	//=============================================================================================//
	// SRV生成

	D3D12_SHADER_RESOURCE_VIEW_DESC instancingSrvDesc{};

	instancingSrvDesc.Format = DXGI_FORMAT_UNKNOWN;
	instancingSrvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	instancingSrvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
	instancingSrvDesc.Buffer.FirstElement = 0;
	instancingSrvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;
	instancingSrvDesc.Buffer.NumElements = kNumInstance;
	instancingSrvDesc.Buffer.StructureByteStride = sizeof (TransformationMatrix);

	D3D12_CPU_DESCRIPTOR_HANDLE instancingSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandle (kInstancingSrvIndex);

	instancingSrvHandleGPU_ = srvDescriptorHeap->GetGPUDescriptorHandle (kInstancingSrvIndex);

	device->CreateShaderResourceView (instancingResource_.Get (), &instancingSrvDesc, instancingSrvHandleCPU);

	//=============================================================================================//
	// Transform初期化（少しずつずらす）

	for (uint32_t index = 0; index < kNumInstance; ++index) {

		transforms_[index].scale = { 1.0f,1.0f,1.0f };
		transforms_[index].rotate = { 0.0f,0.0f,0.0f };
		transforms_[index].translate = { index * 0.1f,index * 0.1f,index * 0.1f };
	}
}

//=================================================================================================//
// 更新処理

void Particle::Update (const Matrix4x4& viewProjectionMatrix) {

	//=============================================================================================//
	// WVPを計算してResourceに書き込む

	for (uint32_t index = 0; index < kNumInstance; ++index) {

		Matrix4x4 worldMatrix =
			MathUtility::MakeAffineMatrix (transforms_[index].scale, transforms_[index].rotate, transforms_[index].translate);

		Matrix4x4 worldViewProjectionMatrix = MathUtility::Multiply (worldMatrix, viewProjectionMatrix);

		instancingData_[index].WVP = worldViewProjectionMatrix;

		instancingData_[index].World = worldMatrix;
	}
}

//=================================================================================================//
// 描画処理

void Particle::Draw () {

	// 確保したResourceの数を超えて描画しない
	assert (numInstance_ <= kNumInstance);

	ID3D12GraphicsCommandList* commandList = GraphicsSystem::GetCommandList ();

	GraphicsSystem* graphicsSystem = GraphicsSystem::GetGraphicsSystem ();

	//=============================================================================================//
	// 描画前設定

	graphicsSystem->PreParticleDraw (
		commandList,
		mesh_->GetVertexBufferView (),
		mesh_->GetIndexBufferView (),
		blendMode_);

	//=============================================================================================//
	// Material・Texture設定

	material_->BindWithoutLight ();

	//=============================================================================================//
	// Instancing用のStructuredBufferのSRVを設定

	commandList->SetGraphicsRootDescriptorTable (1, instancingSrvHandleGPU_);

	//=============================================================================================//
	// インスタンス数だけ描画

	mesh_->Draw (numInstance_);
}

//=================================================================================================//
// 終了処理

void Particle::Finalize () {

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
// 色設定

void Particle::SetColor (const Vector4& color) {

	material_->SetColor (color);
}

//=================================================================================================//
// Texture設定

void Particle::SetTexture (const std::string& textureName) {

	uint32_t textureIndex = GraphicsSystem::GetGraphicsSystem ()->GetTextureIndex (textureName);

	material_->SetTextureIndex (textureIndex);
}

//=================================================================================================//
// BlendMode設定

void Particle::SetBlendMode (BlendMode blendMode) {

	blendMode_ = blendMode;
}
