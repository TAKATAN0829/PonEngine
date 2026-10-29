#include "Model.h"

// C++
#include <cassert>

// assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 自作
#include "ModelCommon.h"
#include "TextureManager.h"

//=================================================================================================//
// 初期化処理

void Model::Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename) {

	assert(modelCommon);

	modelCommon_ = modelCommon;

	//=============================================================================================//
	// モデル読み込み

	modelData_ = LoadModelFile(directoryPath, filename);

	// 全頂点を一度だけ走査し、オブジェクトの選択に使うローカル範囲を保持する。
	if (!modelData_.vertices.empty()) {
		const Vector4& firstPosition = modelData_.vertices.front().position;
		localBounds_ = {{firstPosition.x, firstPosition.y, firstPosition.z},
			{firstPosition.x, firstPosition.y, firstPosition.z}};
		for (const Mesh::VertexData& vertex : modelData_.vertices) {
			if (vertex.position.x < localBounds_.min.x) { localBounds_.min.x = vertex.position.x; }
			if (vertex.position.y < localBounds_.min.y) { localBounds_.min.y = vertex.position.y; }
			if (vertex.position.z < localBounds_.min.z) { localBounds_.min.z = vertex.position.z; }
			if (vertex.position.x > localBounds_.max.x) { localBounds_.max.x = vertex.position.x; }
			if (vertex.position.y > localBounds_.max.y) { localBounds_.max.y = vertex.position.y; }
			if (vertex.position.z > localBounds_.max.z) { localBounds_.max.z = vertex.position.z; }
		}
	}

	//=============================================================================================//
	// 頂点データの初期化

	mesh_ = std::make_unique<Mesh>();

	mesh_->Initialize(modelCommon_->GetDxCommon(), modelData_.vertices);

	//=============================================================================================//
	// マテリアルの初期化

	material_ = std::make_unique<Material>();

	material_->Initialize(modelCommon_->GetDxCommon(), modelCommon_->GetSrvManager());

	//=============================================================================================//
	// テクスチャ読み込み

	if (!modelData_.material.textureFilePath.empty()) {

		modelData_.material.textureIndex = TextureManager::GetInstance()->Load(modelData_.material.textureFilePath);
	}

	material_->SetTextureIndex(modelData_.material.textureIndex);
}

//=================================================================================================//
// 描画処理

void Model::Draw(Material* instanceMaterial) {

	//=============================================================================================//
	// Material・Texture設定

	if (instanceMaterial != nullptr) {
		// Object3dから呼ばれた場合は、共有モデルの設定ではなく個別の見た目を使う。
		instanceMaterial->Bind();
	} else {
		material_->Bind();
	}

	//=============================================================================================//
	// VBV設定・DrawCall

	mesh_->Draw();
}

//=================================================================================================//
// モデルファイル読み込み

Model::ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {

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

				Mesh::VertexData vertex{};

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

Model::Node Model::ReadNode(aiNode* node) {

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
