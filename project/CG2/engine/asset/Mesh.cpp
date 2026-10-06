#include "Mesh.h"

// C++
#include <numbers>
#include <cmath>
#include <cstring>
#include <fstream>
#include <sstream>
#include <cassert>

// 自作
#include "DirectXCommon.h"

//=================================================================================================//
// 初期化処理

void Mesh::Initialize (DirectXCommon* dxCommon, MeshType meshType) {

	assert (dxCommon);

	dxCommon_ = dxCommon;

	switch (meshType) {
	case MeshType::kSphere:

	InitializeSphere ();

	break;

	case MeshType::kPlane:

	InitializePlane ();

	break;

	case MeshType::kSprite:

	InitializeSprite ();

	break;

	case MeshType::kTriangle:

	InitializeTriangle ();

	break;
	}
}

//=================================================================================================//
// 初期化処理(OBJモデル)

void Mesh::Initialize (DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename) {

	assert (dxCommon);

	dxCommon_ = dxCommon;

	//=============================================================================================//
	// OBJファイル読み込み

	ModelData modelData = LoadObjFile (directoryPath, filename);

	//=============================================================================================//
	// Vertex数・Index数設定

	vertexCount_ = static_cast<uint32_t>(modelData.vertices.size ());

	indexCount_ = vertexCount_;

	//=============================================================================================//
	// VertexResource・IndexResource生成

	CreateVertexResource ();

	CreateIndexResource ();

	//=============================================================================================//
	// VertexData・IndexDataを書き込む

	std::memcpy (
		vertexData_,
		modelData.vertices.data (),
		sizeof (VertexData) * vertexCount_);

	for (uint32_t index = 0; index < indexCount_; ++index) {

		indexData_[index] = index;
	}
}

//=================================================================================================//
// 描画処理

void Mesh::Draw (uint32_t instanceCount) {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList ();

	//=============================================================================================//
	// VBV設定

	commandList->IASetVertexBuffers (
		0,
		1,
		&vertexBufferView_);

	//=============================================================================================//
	// IBV設定

	commandList->IASetIndexBuffer (&indexBufferView_);

	//=============================================================================================//
	// 描画

	commandList->DrawIndexedInstanced (
		indexCount_,
		instanceCount,
		0,
		0,
		0);
}

//=================================================================================================//
// Sphere初期化

void Mesh::InitializeSphere () {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = (kSubdivision + 1) * (kSubdivision + 1);

	//=============================================================================================//
	// Index数設定

	indexCount_ = kSubdivision * kSubdivision * 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource ();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource ();

	//=============================================================================================//
	// Sphere生成

	CreateSphere ();
}

//=================================================================================================//
// Plane初期化

void Mesh::InitializePlane () {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 4;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource ();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource ();

	//=============================================================================================//
	// Plane生成

	CreatePlane ();
}

//=================================================================================================//
// Sprite初期化

void Mesh::InitializeSprite () {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 4;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource ();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource ();

	//=============================================================================================//
	// Sprite生成

	CreateSprite ();
}

//=================================================================================================//
// Triangle初期化

void Mesh::InitializeTriangle () {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 3;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource ();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource ();

	//=============================================================================================//
	// Triangle生成

	CreateTriangle ();
}

//=================================================================================================//
// 頂点Resource生成

void Mesh::CreateVertexResource () {

	//=============================================================================================//
	// VertexResource生成

	vertexResource_ = ResourceManager::CreateBufferResource (
		dxCommon_->GetDevice (),
		sizeof (VertexData) * vertexCount_,
		"vertexResource");

	//=============================================================================================//
	// VertexBufferView作成

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress ();

	vertexBufferView_.SizeInBytes = sizeof (VertexData) * vertexCount_;

	vertexBufferView_.StrideInBytes = sizeof (VertexData);

	//=============================================================================================//
	// VertexDataを書き込む

	vertexResource_->Map (
		0,
		nullptr,
		reinterpret_cast<void**>(&vertexData_));
}

//=================================================================================================//
// IndexResource生成

void Mesh::CreateIndexResource () {

	//=============================================================================================//
	// IndexResource生成

	indexResource_ = ResourceManager::CreateBufferResource (
		dxCommon_->GetDevice (),
		sizeof (uint32_t) * indexCount_,
		"indexResource");

	//=============================================================================================//
	// IndexBufferView作成

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress ();

	indexBufferView_.SizeInBytes = sizeof (uint32_t) * indexCount_;

	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	//=============================================================================================//
	// IndexDataを書き込む

	indexResource_->Map (
		0,
		nullptr,
		reinterpret_cast<void**>(&indexData_));
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
	indexData_[1] = 1;
	indexData_[2] = 2;

	indexData_[3] = 1;
	indexData_[4] = 3;
	indexData_[5] = 2;
}

//=================================================================================================//
// Sprite生成

void Mesh::CreateSprite () {

	//=============================================================================================//
	// 左上

	vertexData_[0].position = { -1.0f,1.0f,0.0f,1.0f };
	vertexData_[0].texcoord = { 0.0f,1.0f };
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 右上

	vertexData_[1].position = { 1.0f,1.0f,0.0f,1.0f };
	vertexData_[1].texcoord = { 1.0f,1.0f };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 左下

	vertexData_[2].position = { -1.0f,-1.0f,0.0f,1.0f };
	vertexData_[2].texcoord = { 0.0f,0.0f };
	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 右下

	vertexData_[3].position = { 1.0f,-1.0f,0.0f,1.0f };
	vertexData_[3].texcoord = { 1.0f,0.0f };
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

	// 表面
	indexData_[0] = 0;
	indexData_[1] = 1;
	indexData_[2] = 2;

	// 裏面
	indexData_[3] = 0;
	indexData_[4] = 2;
	indexData_[5] = 1;
 }

//=================================================================================================//
// OBJファイル読み込み

ModelData Mesh::LoadObjFile (const std::string& directoryPath, const std::string& filename) {

	ModelData modelData;

	std::vector<Vector4> positions;
	std::vector<Vector3> normals;
	std::vector<Vector2> texcoords;

	std::string line;

	// ファイルを開く
	std::ifstream file (directoryPath + "/" + filename);

	assert (file.is_open ());

	// ファイルを読む
	while (std::getline (file, line)) {

		std::string identifier;

		std::istringstream s (line);

		s >> identifier;

		if (identifier == "v") {

			Vector4 position{};

			s >> position.x >> position.y >> position.z;

			position.x *= -1.0f;
			position.w = 1.0f;

			positions.push_back (position);

		} else if (identifier == "vt") {

			Vector2 texcoord{};

			s >> texcoord.x >> texcoord.y;

			texcoord.y = 1.0f - texcoord.y;

			texcoords.push_back (texcoord);

		} else if (identifier == "vn") {

			Vector3 normal{};

			s >> normal.x >> normal.y >> normal.z;

			normal.x *= -1.0f;

			normals.push_back (normal);

		} else if (identifier == "f") {

			VertexData triangle[3]{};

			for (int32_t faceVertex = 0; faceVertex < 3; ++faceVertex) {

				std::string vertexDefinition;

				s >> vertexDefinition;

				std::istringstream v (vertexDefinition);

				uint32_t elementIndices[3]{};

				for (int32_t element = 0; element < 3; ++element) {

					std::string index;

					std::getline (
						v,
						index,
						'/');

					if (!index.empty ()) {

						elementIndices[element] = std::stoi (index);
					}
				}

				triangle[faceVertex].position = positions[elementIndices[0] - 1];

				if (elementIndices[1] != 0) {

					triangle[faceVertex].texcoord = texcoords[elementIndices[1] - 1];
				}

				if (elementIndices[2] != 0) {

					triangle[faceVertex].normal = normals[elementIndices[2] - 1];
				}
			}

			// 頂点を逆順で登録して回り順を逆にする
			modelData.vertices.push_back (triangle[2]);
			modelData.vertices.push_back (triangle[1]);
			modelData.vertices.push_back (triangle[0]);
		}
	}

	return modelData;
}