#include "Mesh.h"

// C++
#include <numbers>
#include <cmath>

//=================================================================================================//
// 初期化処理

void Mesh::InitializeSphere (
	ID3D12Device* device,
	ResourceManager* resourceManager) {

	//=============================================================================================//
	// VertexResource生成

	vertexResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (VertexData) * kVertexCount,
			"vertexResource");

	//=============================================================================================//
	// VertexBufferView作成

	vertexBufferView_.BufferLocation =
		vertexResource_->GetGPUVirtualAddress ();

	vertexBufferView_.SizeInBytes =
		sizeof (VertexData) * kVertexCount;

	vertexBufferView_.StrideInBytes =
		sizeof (VertexData);

	//=============================================================================================//
	// VertexDataを書き込む

	vertexResource_->Map (
		0,
		nullptr,
		reinterpret_cast<void**>(&vertexData_));

	//=============================================================================================//
	// IndexResource生成

	indexResource_ =
		resourceManager->CreateBufferResource (
			device,
			sizeof (uint32_t) * kIndexCount,
			"indexResource");

	//=============================================================================================//
	// IndexBufferView作成

	indexBufferView_.BufferLocation =
		indexResource_->GetGPUVirtualAddress ();

	indexBufferView_.SizeInBytes =
		sizeof (uint32_t) * kIndexCount;

	indexBufferView_.Format =
		DXGI_FORMAT_R32_UINT;

	//=============================================================================================//
	// IndexDataを書き込む

	indexResource_->Map (
		0,
		nullptr,
		reinterpret_cast<void**>(&indexData_));

	//=============================================================================================//
	// Sphere生成

	CreateSphere ();
}

//=================================================================================================//
// VertexBufferView取得

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

	return kIndexCount;
}

//=================================================================================================//
// Sphere生成

void Mesh::CreateSphere () {

	//=============================================================================================//
	// 頂点生成

	const float kLonEvery =
		std::numbers::pi_v<float> *2.0f / float (kSubdivision);

	const float kLatEvery =
		std::numbers::pi_v<float> / float (kSubdivision);

	for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex) {

		float lat =
			-std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;

		for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) {

			uint32_t index =
				latIndex * (kSubdivision + 1) + lonIndex;

			float lon =
				lonIndex * kLonEvery;

			vertexData_[index].position.x =
				std::cos (lat) * std::cos (lon);

			vertexData_[index].position.y =
				std::sin (lat);

			vertexData_[index].position.z =
				std::cos (lat) * std::sin (lon);

			vertexData_[index].position.w =
				1.0f;

			vertexData_[index].texcoord = {
				float (lonIndex) / float (kSubdivision),
				1.0f - float (latIndex) / float (kSubdivision)
			};

			vertexData_[index].normal.x =
				vertexData_[index].position.x;

			vertexData_[index].normal.y =
				vertexData_[index].position.y;

			vertexData_[index].normal.z =
				vertexData_[index].position.z;
		}
	}

	//=============================================================================================//
	// Index生成

	for (uint32_t latIndex = 0; latIndex < kSubdivision; ++latIndex) {

		for (uint32_t lonIndex = 0; lonIndex < kSubdivision; ++lonIndex) {

			uint32_t start =
				(latIndex * (kSubdivision + 1)) + lonIndex;

			uint32_t index =
				(latIndex * kSubdivision + lonIndex) * 6;

			indexData_[index + 0] =
				start;

			indexData_[index + 1] =
				start + kSubdivision + 1;

			indexData_[index + 2] =
				start + 1;

			indexData_[index + 3] =
				start + 1;

			indexData_[index + 4] =
				start + kSubdivision + 1;

			indexData_[index + 5] =
				start + kSubdivision + 2;
		}
	}
}