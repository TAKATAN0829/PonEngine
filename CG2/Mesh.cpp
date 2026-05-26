#include "Mesh.h"

// C++
#include <numbers>
#include <cmath>

//=================================================================================================//
// 初期化処理

void Mesh::Initialize (ID3D12Device* device, ResourceManager* resourceManager, MeshType meshType) {

	switch (meshType) {
	case MeshType::kSphere:

	InitializeSphere (device, resourceManager);

	break;

	case MeshType::kPlane:

	InitializePlane (device, resourceManager);

	break;

	case MeshType::kTriangle:

	InitializeTriangle (device,	resourceManager);

	break;
	}
}

//=================================================================================================//
// 描画処理

void Mesh::Draw (ID3D12GraphicsCommandList* commandList) {

	//=============================================================================================//
	// VBV設定

	commandList->IASetVertexBuffers (0, 1, &vertexBufferView_);

	//=============================================================================================//
	// IBV設定

	commandList->IASetIndexBuffer (&indexBufferView_);

	//=============================================================================================//
	// 描画

	commandList->DrawIndexedInstanced (indexCount_, 1, 0, 0, 0);
}

//=================================================================================================//
// Sphere初期化

void Mesh::InitializeSphere (ID3D12Device* device, ResourceManager* resourceManager) {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = (kSubdivision + 1) * (kSubdivision + 1);

	//=============================================================================================//
	// Index数設定

	indexCount_ = kSubdivision * kSubdivision * 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource (device, resourceManager);

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource (device, resourceManager);

	//=============================================================================================//
	// Sphere生成

	CreateSphere ();
}

//=================================================================================================//
// Plane初期化

void Mesh::InitializePlane (ID3D12Device* device, ResourceManager* resourceManager) {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 4;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource (device, resourceManager);

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource (device, resourceManager);

	//=============================================================================================//
	// Plane生成

	CreatePlane ();
}

//=================================================================================================//
// Triangle初期化

void Mesh::InitializeTriangle (ID3D12Device* device, ResourceManager* resourceManager) {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 3;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 3;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource (device, resourceManager);

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource (device, resourceManager);

	//=============================================================================================//
	// Triangle生成

	CreateTriangle ();
}

//=================================================================================================//
// 頂点BufferView取得

D3D12_VERTEX_BUFFER_VIEW* Mesh::GetVertexBufferView () {

	return &vertexBufferView_;
}

//=================================================================================================//
// IndexBufferView取得

D3D12_INDEX_BUFFER_VIEW* Mesh::GetIndexBufferView () {

	return &indexBufferView_;
}

//=================================================================================================//
// Index数取得

uint32_t Mesh::GetIndexCount () {

	return indexCount_;
}

//=================================================================================================//
// 頂点Resource生成

void Mesh::CreateVertexResource (ID3D12Device* device, ResourceManager* resourceManager) {

	//=============================================================================================//
	// VertexResource生成

	vertexResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (VertexData) * vertexCount_,
			"vertexResource");

	//=============================================================================================//
	// VertexBufferView作成

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress ();

	vertexBufferView_.SizeInBytes = sizeof (VertexData) * vertexCount_;

	vertexBufferView_.StrideInBytes = sizeof (VertexData);

	//=============================================================================================//
	// VertexDataを書き込む

	vertexResource_->Map (0, nullptr, reinterpret_cast<void**>(&vertexData_));
}

//=================================================================================================//
// IndexResource生成

void Mesh::CreateIndexResource (ID3D12Device* device, ResourceManager* resourceManager) {

	//=============================================================================================//
	// IndexResource生成

	indexResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (uint32_t) * indexCount_,
			"indexResource");

	//=============================================================================================//
	// IndexBufferView作成

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress ();

	indexBufferView_.SizeInBytes = sizeof (uint32_t) * indexCount_;

	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	//=============================================================================================//
	// IndexDataを書き込む

	indexResource_->Map (0, nullptr, reinterpret_cast<void**>(&indexData_));
}

//=================================================================================================//
// Sphere生成

void Mesh::CreateSphere () {

	//=============================================================================================//
	// 経度分割1つ分の角度

	const float kLonEvery = std::numbers::pi_v<float> *2.0f / float (kSubdivision);

	//=============================================================================================//
	// 緯度分割1つ分の角度

	const float kLatEvery = std::numbers::pi_v<float> / float (kSubdivision);

	//=============================================================================================//
	// 頂点生成

	for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex) {

		float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;

		for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) {

			uint32_t index = latIndex * (kSubdivision + 1) + lonIndex;

			float lon = lonIndex * kLonEvery;

			vertexData_[index].position.x = std::cos (lat) * std::cos (lon);

			vertexData_[index].position.y = std::sin (lat);

			vertexData_[index].position.z = std::cos (lat) * std::sin (lon);

			vertexData_[index].position.w = 1.0f;

			vertexData_[index].texcoord = {
				float (lonIndex) / float (kSubdivision),
				1.0f - float (latIndex) / float (kSubdivision)
			};

			vertexData_[index].normal.x = vertexData_[index].position.x;

			vertexData_[index].normal.y = vertexData_[index].position.y;

			vertexData_[index].normal.z = vertexData_[index].position.z;
		}
	}

	//=============================================================================================//
	// Index生成

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {

			uint32_t start = latIndex * (kSubdivision + 1) + lonIndex;

			uint32_t index = (latIndex * kSubdivision + lonIndex) * 6;

			indexData_[index + 0] = start;

			indexData_[index + 1] = start + kSubdivision + 1;

			indexData_[index + 2] = start + 1;

			indexData_[index + 3] = start + 1;

			indexData_[index + 4] = start + kSubdivision + 1;

			indexData_[index + 5] = start + kSubdivision + 2;
		}
	}
}

//=================================================================================================//
// Plane生成

void Mesh::CreatePlane () {

	//=============================================================================================//
	// 左上

	vertexData_[0].position = { -1.0f,1.0f,0.0f,1.0f };

	vertexData_[0].texcoord = { 0.0f,0.0f };

	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 右上

	vertexData_[1].position = { 1.0f,1.0f,0.0f,1.0f };

	vertexData_[1].texcoord = { 1.0f,0.0f };

	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 左下

	vertexData_[2].position = { -1.0f,-1.0f,0.0f,1.0f };

	vertexData_[2].texcoord = { 0.0f,1.0f };

	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 右下

	vertexData_[3].position = { 1.0f,-1.0f,0.0f,1.0f };

	vertexData_[3].texcoord = { 1.0f,1.0f };

	vertexData_[3].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// Index

	indexData_[0] = 0;
	indexData_[1] = 2;
	indexData_[2] = 1;

	indexData_[3] = 1;
	indexData_[4] = 2;
	indexData_[5] = 3;
}

//=================================================================================================//
// Triangle生成

void Mesh::CreateTriangle () {

	//=============================================================================================//
	// 上

	vertexData_[0].position = { 0.0f,1.0f,0.0f,1.0f };

	vertexData_[0].texcoord = { 0.5f,0.0f };

	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 左下

	vertexData_[1].position = { -1.0f,-1.0f,0.0f,1.0f };

	vertexData_[1].texcoord = { 0.0f,1.0f };

	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 右下

	vertexData_[2].position = { 1.0f,-1.0f,0.0f,1.0f };

	vertexData_[2].texcoord = { 1.0f,1.0f };

	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// Index

	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;
}