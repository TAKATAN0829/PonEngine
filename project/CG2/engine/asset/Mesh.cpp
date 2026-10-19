#include "Mesh.h"

// C++
#include <numbers>
#include <cmath>
#include <cstring>
#include <cassert>

// assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 自作
#include "DirectXCommon.h"

//=================================================================================================//
// 初期化処理

void Mesh::Initialize(DirectXCommon* dxCommon, MeshType meshType) {

	assert(dxCommon);

	dxCommon_ = dxCommon;

	switch (meshType) {
	case MeshType::kSphere:

		InitializeSphere();

		break;

	case MeshType::kPlane:

		InitializePlane();

		break;

	case MeshType::kSprite:

		InitializeSprite();

		break;

	case MeshType::kTriangle:

		InitializeTriangle();

		break;
	}
}

//=================================================================================================//
// 初期化処理(モデル)

void Mesh::Initialize(DirectXCommon* dxCommon, const std::string& directoryPath, const std::string& filename) {

	assert(dxCommon);

	dxCommon_ = dxCommon;

	//=============================================================================================//
	// モデルファイル読み込み

	ModelData modelData = LoadModelFile(directoryPath, filename);

	material_ = modelData.material;

	rootNode_ = modelData.rootNode;

	//=============================================================================================//
	// Vertex数・Index数設定

	vertexCount_ = static_cast<uint32_t>(modelData.vertices.size());

	indexCount_ = vertexCount_;

	//=============================================================================================//
	// VertexResource・IndexResource生成

	CreateVertexResource();

	CreateIndexResource();

	//=============================================================================================//
	// VertexData・IndexDataを書き込む

	std::memcpy(vertexData_, modelData.vertices.data(), sizeof(VertexData) * vertexCount_);

	for (uint32_t index = 0; index < indexCount_; ++index) {

		indexData_[index] = index;
	}
}

//=================================================================================================//
// 描画処理

void Mesh::Draw(uint32_t instanceCount) {

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	//=============================================================================================//
	// VBV設定

	commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);

	//=============================================================================================//
	// IBV設定

	commandList->IASetIndexBuffer(&indexBufferView_);

	//=============================================================================================//
	// 描画

	commandList->DrawIndexedInstanced(
		indexCount_,
		instanceCount,
		0,
		0,
		0);
}

//=================================================================================================//
// Sphere初期化

void Mesh::InitializeSphere() {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = (kSubdivision + 1) * (kSubdivision + 1);

	//=============================================================================================//
	// Index数設定

	indexCount_ = kSubdivision * kSubdivision * 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource();

	//=============================================================================================//
	// Sphere生成

	CreateSphere();
}

//=================================================================================================//
// Plane初期化

void Mesh::InitializePlane() {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 4;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource();

	//=============================================================================================//
	// Plane生成

	CreatePlane();
}

//=================================================================================================//
// Sprite初期化

void Mesh::InitializeSprite() {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 4;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource();

	//=============================================================================================//
	// Sprite生成

	CreateSprite();
}

//=================================================================================================//
// Triangle初期化

void Mesh::InitializeTriangle() {

	//=============================================================================================//
	// Vertex数設定

	vertexCount_ = 3;

	//=============================================================================================//
	// Index数設定

	indexCount_ = 6;

	//=============================================================================================//
	// VertexResource生成

	CreateVertexResource();

	//=============================================================================================//
	// IndexResource生成

	CreateIndexResource();

	//=============================================================================================//
	// Triangle生成

	CreateTriangle();
}

//=================================================================================================//
// 頂点Resource生成

void Mesh::CreateVertexResource() {

	//=============================================================================================//
	// VertexResource生成

	vertexResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(VertexData) * vertexCount_, "vertexResource");

	//=============================================================================================//
	// VertexBufferView作成

	vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();

	vertexBufferView_.SizeInBytes = sizeof(VertexData) * vertexCount_;

	vertexBufferView_.StrideInBytes = sizeof(VertexData);

	//=============================================================================================//
	// VertexDataを書き込む

	vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));
}

//=================================================================================================//
// IndexResource生成

void Mesh::CreateIndexResource() {

	//=============================================================================================//
	// IndexResource生成

	indexResource_ = ResourceManager::CreateBufferResource(dxCommon_->GetDevice(), sizeof(uint32_t) * indexCount_, "indexResource");

	//=============================================================================================//
	// IndexBufferView作成

	indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();

	indexBufferView_.SizeInBytes = sizeof(uint32_t) * indexCount_;

	indexBufferView_.Format = DXGI_FORMAT_R32_UINT;

	//=============================================================================================//
	// IndexDataを書き込む

	indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
}

//=================================================================================================//
// Sphere生成

void Mesh::CreateSphere() {

	//=============================================================================================//
	// 経度分割1つ分の角度

	const float kLonEvery = std::numbers::pi_v<float> *2.0f / float(kSubdivision);

	//=============================================================================================//
	// 緯度分割1つ分の角度

	const float kLatEvery = std::numbers::pi_v<float> / float(kSubdivision);

	//=============================================================================================//
	// 頂点生成

	for (uint32_t latIndex = 0; latIndex <= kSubdivision; ++latIndex) {

		float lat = -std::numbers::pi_v<float> / 2.0f + kLatEvery * latIndex;

		for (uint32_t lonIndex = 0; lonIndex <= kSubdivision; ++lonIndex) {

			uint32_t index = latIndex * (kSubdivision + 1) + lonIndex;

			float lon = lonIndex * kLonEvery;

			vertexData_[index].position.x = std::cos(lat) * std::cos(lon);

			vertexData_[index].position.y = std::sin(lat);

			vertexData_[index].position.z = std::cos(lat) * std::sin(lon);

			vertexData_[index].position.w = 1.0f;

			vertexData_[index].texcoord = {
				float(lonIndex) / float(kSubdivision),
				1.0f - float(latIndex) / float(kSubdivision)
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

void Mesh::CreatePlane() {

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

void Mesh::CreateSprite() {

	//=============================================================================================//
	// 左上

	vertexData_[0].position = { 0.0f,0.0f,0.0f,1.0f };
	vertexData_[0].texcoord = { 0.0f,0.0f };
	vertexData_[0].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 右上

	vertexData_[1].position = { 1.0f,0.0f,0.0f,1.0f };
	vertexData_[1].texcoord = { 1.0f,0.0f };
	vertexData_[1].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 左下

	vertexData_[2].position = { 0.0f,1.0f,0.0f,1.0f };
	vertexData_[2].texcoord = { 0.0f,1.0f };
	vertexData_[2].normal = { 0.0f,0.0f,-1.0f };

	//=============================================================================================//
	// 右下

	vertexData_[3].position = { 1.0f,1.0f,0.0f,1.0f };
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
// Triangle生成

void Mesh::CreateTriangle() {

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
// モデルファイル読み込み

ModelData Mesh::LoadModelFile(const std::string& directoryPath, const std::string& filename) {

	ModelData modelData;

	Assimp::Importer importer;

	std::string filePath = directoryPath + "/" + filename;

	const aiScene* scene = importer.ReadFile(filePath.c_str(), aiProcess_FlipWindingOrder | aiProcess_FlipUVs);

	assert(scene);
	assert(scene->HasMeshes());

	//=============================================================================================//
	// Mesh解析

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {

		aiMesh* mesh = scene->mMeshes[meshIndex];

		assert(mesh->HasNormals());
		assert(mesh->HasTextureCoords(0));

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {

			aiFace& face = mesh->mFaces[faceIndex];

			assert(face.mNumIndices == 3);

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {

				uint32_t vertexIndex = face.mIndices[element];

				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D& texcoord = mesh->mTextureCoords[0][vertexIndex];

				VertexData vertex{};

				vertex.position = { -position.x,position.y,position.z,1.0f };
				vertex.normal = { -normal.x,normal.y,normal.z };
				vertex.texcoord = { texcoord.x,texcoord.y };

				modelData.vertices.push_back(vertex);
			}
		}
	}

	//=============================================================================================//
	// Material解析

	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {

		aiMaterial* material = scene->mMaterials[materialIndex];

		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {

			aiString textureFilePath;

			material->GetTexture(aiTextureType_DIFFUSE, 0, &textureFilePath);

			modelData.material.textureFilePath = directoryPath + "/" + textureFilePath.C_Str();
		}
	}

	//=============================================================================================//
	// Node解析

	modelData.rootNode = ReadNode(scene->mRootNode);

	return modelData;
}

//=================================================================================================//
// Node読み込み

Node Mesh::ReadNode(aiNode* node) {

	Node result;

	aiMatrix4x4 aiLocalMatrix = node->mTransformation;

	aiLocalMatrix.Transpose();

	for (uint32_t row = 0; row < 4; ++row) {

		for (uint32_t column = 0; column < 4; ++column) {

			result.localMatrix.m[row][column] = aiLocalMatrix[row][column];
		}
	}

	result.name = node->mName.C_Str();

	result.children.resize(node->mNumChildren);

	for (uint32_t childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {

		result.children[childIndex] = ReadNode(node->mChildren[childIndex]);
	}

	return result;
}
