#include "ModelAnimation.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>

namespace {
	// ゼロ拡縮で逆行列が作れないフレームでも、GPUへNaNの法線を渡さない。
	aiMatrix3x3 MakeNormalMatrix(const aiMatrix4x4& matrix) {
		aiMatrix3x3 normal(matrix);
		if (std::abs(normal.Determinant()) < 0.00000001f) {
			return aiMatrix3x3();
		}
		return normal.Inverse().Transpose();
	}
	// キー時刻を二分探索し、端の時刻でも範囲外のキーを参照しない。
	template<class KeyType>
	size_t FindKey(const KeyType* keys, size_t count, double time) {
		size_t lower = 0;
		size_t upper = count - 1;
		while (lower + 1 < upper) {
			const size_t middle = (lower + upper) / 2;
			if (keys[middle].mTime <= time) {
				lower = middle;
			} else {
				upper = middle;
			}
		}
		return lower;
	}

	aiVector3D SampleVector(const aiVectorKey* keys, size_t count, double time, const aiVector3D& fallback) {
		if (count == 0) {
			return fallback;
		}
		if (count == 1 || time <= keys[0].mTime) {
			return keys[0].mValue;
		}
		if (time >= keys[count - 1].mTime) {
			return keys[count - 1].mValue;
		}
		const size_t index = FindKey(keys, count, time);
		const double interval = keys[index + 1].mTime - keys[index].mTime;
		if (interval <= 0) {
			return keys[index + 1].mValue;
		}
		const float fraction = static_cast<float>((time - keys[index].mTime) / interval);
		return keys[index].mValue + (keys[index + 1].mValue - keys[index].mValue) * fraction;
	}

	aiQuaternion SampleRotation(const aiQuatKey* keys, size_t count, double time, const aiQuaternion& fallback) {
		if (count == 0) {
			return fallback;
		}
		if (count == 1 || time <= keys[0].mTime) {
			return keys[0].mValue;
		}
		if (time >= keys[count - 1].mTime) {
			return keys[count - 1].mValue;
		}
		const size_t index = FindKey(keys, count, time);
		const double interval = keys[index + 1].mTime - keys[index].mTime;
		if (interval <= 0) {
			return keys[index + 1].mValue;
		}
		aiQuaternion rotation;
		aiQuaternion::Interpolate(rotation, keys[index].mValue, keys[index + 1].mValue,
			static_cast<float>((time - keys[index].mTime) / interval));
		return rotation.Normalize();
	}
}

struct ModelAnimation::AnimationData {
	struct Node {
		const aiNode* source = nullptr;
		int parent = -1;
	};
	struct Bone {
		size_t node = 0;
		aiMatrix4x4 offset;
	};
	struct Weight {
		size_t bone = 0;
		float weight = 0;
	};
	struct Vertex {
		aiVector3D position;
		aiVector3D normal;
		aiVector3D uv;
		size_t node = 0;
		std::vector<Weight> weights;
	};
	Assimp::Importer importer;
	const aiScene* scene = nullptr; // importerが所有し、この構造体と同じ寿命。
	std::vector<Node> nodes;
	std::unordered_map<std::string, size_t> nodeIndices;
	std::vector<Bone> bones;
	std::vector<Vertex> vertices;
	std::vector<ModelAnimationInfo> clips;
	std::vector<std::vector<const aiNodeAnim*>> channels;
	std::vector<size_t> meshNodes;

	void AddNode(const aiNode* node, int parent) {
		const size_t index = nodes.size();
		nodes.push_back({node, parent});
		nodeIndices[node->mName.C_Str()] = index;
		for (unsigned int meshIndex = 0; meshIndex < node->mNumMeshes; ++meshIndex) {
			meshNodes.at(node->mMeshes[meshIndex]) = index;
		}
		for (unsigned int childIndex = 0; childIndex < node->mNumChildren; ++childIndex) {
			AddNode(node->mChildren[childIndex], static_cast<int>(index));
		}
	}

	void ReadMesh(size_t meshIndex) {
		const aiMesh& mesh = *scene->mMeshes[meshIndex];
		std::vector<std::vector<Weight>> weights(mesh.mNumVertices);
		for (unsigned int boneIndex = 0; boneIndex < mesh.mNumBones; ++boneIndex) {
			const aiBone& bone = *mesh.mBones[boneIndex];
			if (!nodeIndices.contains(bone.mName.C_Str())) {
				throw std::runtime_error("Animation bone node is missing.");
			}
			const size_t paletteIndex = bones.size();
			bones.push_back({nodeIndices.at(bone.mName.C_Str()), bone.mOffsetMatrix});
			for (unsigned int weightIndex = 0; weightIndex < bone.mNumWeights; ++weightIndex) {
				const aiVertexWeight& weight = bone.mWeights[weightIndex];
				if (weight.mVertexId >= weights.size() || !std::isfinite(weight.mWeight) || weight.mWeight < 0) {
					throw std::runtime_error("Invalid skinning weight.");
				}
				weights[weight.mVertexId].push_back({paletteIndex, weight.mWeight});
			}
		}
		for (unsigned int faceIndex = 0; faceIndex < mesh.mNumFaces; ++faceIndex) {
			const aiFace& face = mesh.mFaces[faceIndex];
			if (face.mNumIndices != 3) {
				throw std::runtime_error("Animation mesh must contain triangles.");
			}
			for (unsigned int element = 0; element < face.mNumIndices; ++element) {
				const unsigned int vertexIndex = face.mIndices[element];
				Vertex vertex;
				vertex.position = mesh.mVertices[vertexIndex];
				vertex.normal = mesh.mNormals[vertexIndex];
				if (mesh.HasTextureCoords(0)) {
					vertex.uv = mesh.mTextureCoords[0][vertexIndex];
				}
				vertex.node = meshNodes[meshIndex];
				vertex.weights = weights[vertexIndex];
				vertices.push_back(std::move(vertex));
			}
		}
	}
};

ModelAnimation::ModelAnimation() = default;
ModelAnimation::~ModelAnimation() = default;

void ModelAnimation::Initialize(const std::string& path) {
	data_ = std::make_unique<AnimationData>();
	data_->scene = data_->importer.ReadFile(path,
		aiProcess_FlipWindingOrder | aiProcess_FlipUVs | aiProcess_Triangulate | aiProcess_GenSmoothNormals);
	if (data_->scene == nullptr) {
		throw std::runtime_error(data_->importer.GetErrorString());
	}
	data_->meshNodes.resize(data_->scene->mNumMeshes);
	data_->AddNode(data_->scene->mRootNode, -1);
	for (size_t meshIndex = 0; meshIndex < data_->scene->mNumMeshes; ++meshIndex) {
		data_->ReadMesh(meshIndex);
	}
	for (unsigned int clipIndex = 0; clipIndex < data_->scene->mNumAnimations; ++clipIndex) {
		const aiAnimation& clip = *data_->scene->mAnimations[clipIndex];
		double ticksPerSecond = clip.mTicksPerSecond;
		if (ticksPerSecond <= 0) {
			ticksPerSecond = 25;
		}
		std::string name = clip.mName.C_Str();
		if (name.empty()) {
			name = "Clip " + std::to_string(clipIndex + 1);
		}
		data_->clips.push_back({name, static_cast<float>(clip.mDuration / ticksPerSecond)});
		std::vector<const aiNodeAnim*> channels(data_->nodes.size(), nullptr);
		for (unsigned int channelIndex = 0; channelIndex < clip.mNumChannels; ++channelIndex) {
			const aiNodeAnim* channel = clip.mChannels[channelIndex];
			if (data_->nodeIndices.contains(channel->mNodeName.C_Str())) {
				channels[data_->nodeIndices.at(channel->mNodeName.C_Str())] = channel;
			}
		}
		data_->channels.push_back(std::move(channels));
	}
}

const std::vector<ModelAnimationInfo>& ModelAnimation::GetClips() const {
	return data_->clips;
}

void ModelAnimation::Update(int clipIndex, float seconds, bool loop, std::vector<Mesh::VertexData>& vertices) const {
	bool hasClip = false;
	double ticks = 0;
	if (clipIndex >= 0) {
		hasClip = static_cast<size_t>(clipIndex) < data_->clips.size();
	}
	if (hasClip) {
		const aiAnimation& clip = *data_->scene->mAnimations[clipIndex];
		double rate = clip.mTicksPerSecond;
		if (rate <= 0) {
			rate = 25;
		}
		ticks = (std::max)(0.0, static_cast<double>(seconds) * rate);
		if (loop && clip.mDuration > 0) {
			ticks = std::fmod(ticks, clip.mDuration);
		} else {
			ticks = (std::min)(ticks, clip.mDuration);
		}
	}
	std::vector<aiMatrix4x4> global(data_->nodes.size());
	for (size_t nodeIndex = 0; nodeIndex < data_->nodes.size(); ++nodeIndex) {
		const AnimationData::Node& node = data_->nodes[nodeIndex];
		aiMatrix4x4 local = node.source->mTransformation;
		if (hasClip) {
			const aiNodeAnim* channel = data_->channels[clipIndex][nodeIndex];
			if (channel != nullptr) {
				aiVector3D scale;
				aiVector3D translation;
				aiQuaternion rotation;
				local.Decompose(scale, rotation, translation);
				scale = SampleVector(channel->mScalingKeys, channel->mNumScalingKeys, ticks, scale);
				translation = SampleVector(channel->mPositionKeys, channel->mNumPositionKeys, ticks, translation);
				rotation = SampleRotation(channel->mRotationKeys, channel->mNumRotationKeys, ticks, rotation);
				local = aiMatrix4x4(scale, rotation, translation);
			}
		}
		global[nodeIndex] = local;
		if (node.parent >= 0) {
			global[nodeIndex] = global[node.parent] * local;
		}
	}
	std::vector<aiMatrix4x4> palette(data_->bones.size());
	std::vector<aiMatrix3x3> normals(data_->bones.size());
	for (size_t boneIndex = 0; boneIndex < data_->bones.size(); ++boneIndex) {
		const AnimationData::Bone& bone = data_->bones[boneIndex];
		palette[boneIndex] = global[bone.node] * bone.offset;
		normals[boneIndex] = MakeNormalMatrix(palette[boneIndex]);
	}
	vertices.resize(data_->vertices.size());
	for (size_t vertexIndex = 0; vertexIndex < data_->vertices.size(); ++vertexIndex) {
		const AnimationData::Vertex& source = data_->vertices[vertexIndex];
		aiVector3D position;
		aiVector3D normal;
		float totalWeight = 0;
		for (const AnimationData::Weight& influence : source.weights) {
			position += (palette[influence.bone] * source.position) * influence.weight;
			normal += (normals[influence.bone] * source.normal) * influence.weight;
			totalWeight += influence.weight;
		}
		if (totalWeight > 0.000001f) {
			position /= totalWeight;
			normal /= totalWeight;
		} else {
			position = global[source.node] * source.position;
			normal = MakeNormalMatrix(global[source.node]) * source.normal;
		}
		if (normal.SquareLength() > 0.00000001f) {
			normal.Normalize();
		}
		vertices[vertexIndex] = {{-position.x, position.y, position.z, 1}, {source.uv.x, source.uv.y}, {-normal.x, normal.y, normal.z}};
	}
}
