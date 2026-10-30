#include <filesystem>
#include <stdexcept>
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
	if (modelData_.vertices.empty()) {
		throw std::runtime_error("Model has no triangle vertices.");
	}

	if (modelData_.animated) {
		animation_ = std::make_unique<ModelAnimation>();
		animation_->Initialize(directoryPath + "/" + filename);
		animation_->Update(-1, 0, false, modelData_.vertices);
		modelData_.rootNode.localMatrix = MathUtility::MakeIdentity4x4();
	}

	// 全頂点を一度だけ走査し、オブジェクトの選択に使うローカル範囲を保持する。
	if (!modelData_.vertices.empty()) {
		const Vector4& firstPosition = modelData_.vertices.front().position;
		localBounds_ = {{firstPosition.x, firstPosition.y, firstPosition.z},
			{firstPosition.x, firstPosition.y, firstPosition.z}};
		for (const Mesh::VertexData& vertex : modelData_.vertices) {
			if (vertex.position.x < localBounds_.min.x) {
				localBounds_.min.x = vertex.position.x;
			}
			if (vertex.position.y < localBounds_.min.y) {
				localBounds_.min.y = vertex.position.y;
			}
			if (vertex.position.z < localBounds_.min.z) {
				localBounds_.min.z = vertex.position.z;
			}
			if (vertex.position.x > localBounds_.max.x) {
				localBounds_.max.x = vertex.position.x;
			}
			if (vertex.position.y > localBounds_.max.y) {
				localBounds_.max.y = vertex.position.y;
			}
			if (vertex.position.z > localBounds_.max.z) {
				localBounds_.max.z = vertex.position.z;
			}
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
	// スロットごとにテクスチャと既定色を保持する。
	for (const ModelMaterialInfo& info : modelData_.materials) {
		uint32_t textureIndex = TextureManager::GetInstance()->GetSrvIndex("resources/white1x1.png");
		std::string textureError;
		if (!info.texture.empty()) {
			if (TextureManager::GetInstance()->TryLoad(info.texture, textureError)) {
				textureIndex = TextureManager::GetInstance()->GetSrvIndex(info.texture);
			}
		}
		modelData_.textureIndices.push_back(textureIndex);
		std::unique_ptr<Material> slot = std::make_unique<Material>();
		slot->Initialize(modelCommon_->GetDxCommon(), modelCommon_->GetSrvManager());
		slot->SetTextureIndex(textureIndex);
		slot->SetColor(info.color);
		materials_.push_back(std::move(slot));
	}
	material_->SetTextureIndex(modelData_.textureIndices.front());
}

// スロットごとの範囲を描画し、別マテリアルの設定を上書きしない。
void Model::Draw(Material* instanceMaterial, const std::vector<std::unique_ptr<Material>>* slots, Mesh* animatedMesh) {
	Mesh* drawable = mesh_.get();
	if (animatedMesh != nullptr) {
		drawable = animatedMesh;
	}
	for (const ModelData::DrawRange& range : modelData_.ranges) {
		Material* material = materials_.at(range.materialIndex).get();
		if (slots != nullptr) {
			material = slots->at(range.materialIndex).get();
		} else if (instanceMaterial != nullptr) {
			material = instanceMaterial;
		}
		material->Bind();
		drawable->DrawRange(range.firstIndex, range.indexCount);
	}
}

//=================================================================================================//
// モデルファイル読み込み

Model::ModelData Model::LoadModelFile(const std::string& directoryPath, const std::string& filename) {

	ModelData modelData;

	Assimp::Importer importer;

	std::string filePath = directoryPath + "/" + filename;

	const aiScene* scene = importer.ReadFile(filePath.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_GenSmoothNormals);
	if (scene == nullptr) {
		throw std::runtime_error(importer.GetErrorString());
	}
	if (!scene->HasMeshes()) {
		throw std::runtime_error("Model has no mesh.");
	}
	if (scene->mRootNode == nullptr) {
		throw std::runtime_error("Model has no root node.");
	}

	modelData.animated = scene->HasAnimations();
	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		if (scene->mMeshes[meshIndex]->HasBones()) {
			modelData.animated = true;
		}
	}
	if (!modelData.animated) {
		// 静的モデルだけを焼き込む。アニメーションのノード階層は残す。
		if (filename != "plane.gltf" && filename != "axis.obj") {
			scene = importer.ApplyPostProcessing(aiProcess_PreTransformVertices);
			if (scene == nullptr) {
				throw std::runtime_error(importer.GetErrorString());
			}
		}
	}
	if (scene->mNumMaterials == 0 || scene->mNumMaterials > 256) {
		throw std::runtime_error("Model supports between 1 and 256 material slots.");
	}

	//=============================================================================================//
	// Mesh解析

	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {

		aiMesh* mesh = scene->mMeshes[meshIndex];
		ModelData::DrawRange range;
		range.firstIndex = static_cast<uint32_t>(modelData.vertices.size());
		range.materialIndex = mesh->mMaterialIndex;

		if (!mesh->HasNormals()) {
			throw std::runtime_error("Cannot generate model normals.");
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {

			aiFace& face = mesh->mFaces[faceIndex];

			if (face.mNumIndices != 3) {
				throw std::runtime_error("Cannot triangulate model.");
			}

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {

				uint32_t vertexIndex = face.mIndices[element];

				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];
				aiVector3D texcoord{};
				if (mesh->HasTextureCoords(0)) {
					texcoord = mesh->mTextureCoords[0][vertexIndex];
				}

				Mesh::VertexData vertex{};

				vertex.position = { -position.x,position.y,position.z,1.0f };
				vertex.normal = { -normal.x,normal.y,normal.z };
				vertex.texcoord = { texcoord.x,texcoord.y };

				modelData.vertices.push_back(vertex);
			}
		}
		range.indexCount = static_cast<uint32_t>(modelData.vertices.size()) - range.firstIndex;
		modelData.ranges.push_back(range);
	}

	//=============================================================================================//
	// Material解析

	for (uint32_t materialIndex = 0; materialIndex < scene->mNumMaterials; ++materialIndex) {
		const aiMaterial* material = scene->mMaterials[materialIndex];
		ModelMaterialInfo info;
		aiString materialName;
		material->Get(AI_MATKEY_NAME, materialName);
		info.name = materialName.C_Str();
		if (info.name.empty()) {
			info.name = "Material " + std::to_string(materialIndex);
		}
		aiColor4D color(1, 1, 1, 1);
		material->Get(AI_MATKEY_COLOR_DIFFUSE, color);
		info.color = {color.r, color.g, color.b, color.a};
		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString texturePath;
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);
			info.texture = (std::filesystem::path(filePath).parent_path() / texturePath.C_Str()).generic_string();
		}
		modelData.materials.push_back(std::move(info));
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
