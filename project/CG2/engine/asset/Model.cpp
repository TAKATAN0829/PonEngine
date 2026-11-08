#include <filesystem>
#include "Model.h"
#include <utility>

// C++
#include <cassert>

// assimp
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

// 自作
#include "ModelCommon.h"
#include "TextureManager.h"

//=============================================================================================//
// 初期化処理

Model::Node::Node()
	: localMatrix(MathUtility::MakeIdentity4x4()) {
	// モデルを読み込む前のノードは、頂点に変換を加えない。
}

//=============================================================================================//
// 初期化処理


bool Model::TryInitialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename, std::string& error) {

	assert(modelCommon);

	modelCommon_ = modelCommon;

	//=============================================================================================//
	// モデル読み込み

	if (!LoadModelFile(directoryPath, filename, modelData_, error)) {
		return false;
	}

	// 描画できる三角形がないモデルは、GPU用のバッファを作る前に拒否する。
	if (modelData_.vertices.empty()) {
		error = "Model has no triangle vertices.";
		return false;
	}

	if (modelData_.isAnimated) {
		animation_ = std::make_unique<ModelAnimation>();

		if (!animation_->TryInitialize(directoryPath + "/" + filename, error)) {
			return false;
		}

		animation_->Update(-1, 0, false, modelData_.vertices);

		modelData_.rootNode.localMatrix = MathUtility::MakeIdentity4x4();
	}

	// 全頂点を一度だけ走査し、オブジェクトの選択に使うローカル範囲を保持する。
	if (!modelData_.vertices.empty()) {
		const Vector4& firstPosition = modelData_.vertices.front().position;

		localBounds_ = {{firstPosition.x, firstPosition.y, firstPosition.z},
			{firstPosition.x, firstPosition.y, firstPosition.z}};

		// 全頂点の各軸の最小値と最大値を集め、選択判定などで使うモデルの範囲を作る。
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

	return true;
}

//=============================================================================================//
// 描画処理


// スロットごとの範囲を描画し、別マテリアルの設定を上書きしない。
void Model::Draw(Material* instanceMaterial, const std::vector<std::unique_ptr<Material>>* slots, Mesh* animatedMesh) {
	// Materialスロットに対応する頂点範囲を順に描画する。
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

//=============================================================================================//
// Getter

Material* Model::GetMaterial() const {
	// モデルが使うMaterialを取得する。
	return material_.get ();
}

const Model::Node& Model::GetRootNode() const {
	// モデルのルートノードを取得する。
	return modelData_.rootNode;
}

// 読込時に求めたメッシュの範囲。画面上の選択判定に使用する。
const AABB& Model::GetLocalBounds() const {
	// 読込時に求めたモデルの範囲を参照で返す。
	return localBounds_;
}

// 元データは共有し、編集値と変形用メッシュはObject3dが個別に所有する。
const std::vector<ModelMaterialInfo>& Model::GetMaterials() const {
	// モデルのMaterial一覧を取得する。
	return modelData_.materials;
}

uint32_t Model::GetTextureIndex(size_t slot) const {
	// 使用するテクスチャの番号を取得する。
	return modelData_.textureIndices.at(slot);
}

const std::vector<Mesh::VertexData>& Model::GetVertices() const {
	// モデルの頂点一覧を取得する。
	return modelData_.vertices;
}

const ModelAnimation* Model::GetAnimation() const {
	// モデルのアニメーション情報を取得する。
	return animation_.get();
}

//=============================================================================================//
// Asset参照の更新

void Model::RemapAssetPaths(const AssetPathChange& change) {
	// キャッシュや保存基準のパスも含め、Asset移動の結果を反映する。
	for (ModelMaterialInfo& material : modelData_.materials) {
		material.texture = change.RemapResource(material.texture);
	}
}

//=============================================================================================//
// モデルファイル読み込み


bool Model::LoadModelFile(const std::string& directoryPath, const std::string& filename, ModelData& modelData, std::string& error) {

	// 読み込んだデータはImporterが所有するため、この関数の中で必要な値を取り出す。
	Assimp::Importer importer;

	// フォルダとファイル名を合わせ、Assimpに渡すパスにする。
	std::string filePath = directoryPath + "/" + filename;

	// Assimpにモデル形式の解釈を任せ、面を三角形にそろえた読込結果を受け取る。
	const aiScene* scene = importer.ReadFile(filePath.c_str(),
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_GenSmoothNormals);

	// 読み込みに失敗するとポインターが空になるため、Assimpの失敗理由を受け取る。
	if (scene == nullptr) {
		error = importer.GetErrorString();
		return false;
	}

	// ファイルが読めても、描画に必要なメッシュと階層の起点がそろっているか確認する。
	if (!scene->HasMeshes()) {
		error = "Model has no mesh.";
		return false;
	}

	// ルートノードがなければ、メッシュをどの座標に置くか階層から求められない。
	if (scene->mRootNode == nullptr) {
		error = "Model has no root node.";
		return false;
	}

	// アニメーションの有無を調べ、静的モデルと読み取り方を分ける。
	modelData.isAnimated = scene->HasAnimations();

	for (unsigned int meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {
		// クリップがなくても、ボーン付きの頂点には階層を使う処理が必要になる。
		if (scene->mMeshes[meshIndex]->HasBones()) {
			modelData.isAnimated = true;
		}
	}

	if (!modelData.isAnimated) {
		// 静的モデルだけを焼き込む。アニメーションのノード階層は残す。
		if (filename != "plane.gltf" && filename != "axis.obj") {
			scene = importer.ApplyPostProcessing(aiProcess_PreTransformVertices);

			if (scene == nullptr) {
				error = importer.GetErrorString();
				return false;
			}
		}
	}

	if (scene->mNumMaterials == 0 || scene->mNumMaterials > 256) {
		error = "Model supports between 1 and 256 material slots.";
		return false;
	}

	//=============================================================================================//
	// Mesh解析

	// Assimpの結果を、エンジンが使う頂点一覧とマテリアル別の描画範囲に変換する。
	for (uint32_t meshIndex = 0; meshIndex < scene->mNumMeshes; ++meshIndex) {

		aiMesh* mesh = scene->mMeshes[meshIndex];

		ModelData::DrawRange range{};

		// 現在の頂点数が、今から追加するメッシュの描画開始位置になる。
		range.firstIndex = static_cast<uint32_t>(modelData.vertices.size());

		// 描画時にこの範囲へ使うマテリアル番号を記録する。
		range.materialIndex = mesh->mMaterialIndex;

		if (!mesh->HasNormals()) {
			error = "Cannot generate model normals.";
			return false;
		}

		for (uint32_t faceIndex = 0; faceIndex < mesh->mNumFaces; ++faceIndex) {

			aiFace& face = mesh->mFaces[faceIndex];

			if (face.mNumIndices != 3) {
				error = "Cannot triangulate model.";
				return false;
			}

			for (uint32_t element = 0; element < face.mNumIndices; ++element) {

				// 面には頂点そのものではなく番号が並ぶので、その番号で位置や法線を引く。
				uint32_t vertexIndex = face.mIndices[element];

				aiVector3D& position = mesh->mVertices[vertexIndex];
				aiVector3D& normal = mesh->mNormals[vertexIndex];

				// UVを持たないメッシュに備え、先に0の座標を用意する。
				aiVector3D texcoord{};

				if (mesh->HasTextureCoords(0)) {
					texcoord = mesh->mTextureCoords[0][vertexIndex];
				}

				Mesh::VertexData vertex{};

				// Xの符号を反転し、読み込んだ座標をエンジンの座標系へ合わせる。
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

		// ファイルのマテリアル名を取り出し、名前がなければ番号で表示名を作る。
		material->Get(AI_MATKEY_NAME, materialName);

		info.name = materialName.C_Str();

		if (info.name.empty()) {
			info.name = "Material " + std::to_string(materialIndex);
		}

		// 色の指定がないマテリアルは白として扱う。
		aiColor4D color(1, 1, 1, 1);

		material->Get(AI_MATKEY_COLOR_DIFFUSE, color);

		info.color = {color.r, color.g, color.b, color.a};

		if (material->GetTextureCount(aiTextureType_DIFFUSE) != 0) {
			aiString texturePath;

			// ベースの色に使う画像のうち、最初に登録されたファイル名を読む。
			material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath);

			// AssimpのUTF-8文字列をANSIへ暗黙変換せず、画像の相対パスを組み立てる。
			const std::filesystem::path textureFile = AssetPathChange::Path(filePath).parent_path() / AssetPathChange::Path(texturePath.C_Str());

			info.texture = AssetPathChange::Text(textureFile);
		}

		modelData.materials.push_back(std::move(info));
	}

	//=============================================================================================//
	// Node解析

	// ルートから子をたどり、Importerの外でも使えるノード階層に変換する。
	modelData.rootNode = ReadNode(scene->mRootNode);

	return true;
}

//=============================================================================================//
// Node読み込み


Model::Node Model::ReadNode(aiNode* node) {
	// ノードの変換と子ノードを読み込み、モデルの階層を組み立てる。

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
