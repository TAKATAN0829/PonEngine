#include "GameScene.h"
#include "objects/WorldPrimitives.h"
#include "ModelManager.h"
#include <cmath>
#include <stdexcept>

namespace {
	Vector3 CrossPrimitiveEdges(const Vector3& first, const Vector3& second) {
		return {first.y*second.z-first.z*second.y,first.z*second.x-first.x*second.z,first.x*second.y-first.y*second.x};
	}
}

// 本番のモデル読込・作成を通し、寸法、法線、巻き順、極の退化面、表面選択を確認する。
void VerifyPrimitiveAssets(GameScene& scene) {
	for (PrimitiveType type : {PrimitiveType::Cube,PrimitiveType::Sphere}) {
		std::string error;
		if (WorldPrimitives::Create(scene,type,error) == 0) {
			throw std::runtime_error("GPU primitive creation failed: "+error);
		}
		const size_t objectIndex = scene.GetEditableObjectCount()-1;
		const EditableObjectView object = scene.GetEditableObject(objectIndex);
		const Model* model = ModelManager::GetInstance()->FindModel(*object.model);
		if (model == nullptr) {
			throw std::runtime_error("Primitive model was not imported.");
		}
		const AABB& bounds = model->GetLocalBounds();
		if (std::abs(bounds.min.x+0.5f)>0.00001f || std::abs(bounds.max.x-0.5f)>0.00001f ||
			std::abs(bounds.min.y+0.5f)>0.00001f || std::abs(bounds.max.y-0.5f)>0.00001f ||
			std::abs(bounds.min.z+0.5f)>0.00001f || std::abs(bounds.max.z-0.5f)>0.00001f) {
			throw std::runtime_error("Primitive is not centered with unit dimensions.");
		}
		const std::vector<Mesh::VertexData>& vertices = model->GetVertices();
		size_t expectedVertices = 36;
		if (type == PrimitiveType::Sphere) {
			expectedVertices = 2880;
		}
		if (vertices.size() != expectedVertices) {
			throw std::runtime_error("Unexpected primitive triangle count.");
		}
		for (size_t vertexIndex = 0; vertexIndex < vertices.size(); vertexIndex += 3) {
			const Vector3 first = {vertices[vertexIndex].position.x,vertices[vertexIndex].position.y,vertices[vertexIndex].position.z};
			const Vector3 second = {vertices[vertexIndex+1].position.x,vertices[vertexIndex+1].position.y,vertices[vertexIndex+1].position.z};
			const Vector3 third = {vertices[vertexIndex+2].position.x,vertices[vertexIndex+2].position.y,vertices[vertexIndex+2].position.z};
			const Vector3 area = CrossPrimitiveEdges(MathUtility::Subtract(second,first),MathUtility::Subtract(third,first));
			if (MathUtility::Length(area)<0.0000001f || MathUtility::Dot(area,vertices[vertexIndex].normal)<=0) {
				throw std::runtime_error("Degenerate or inverted primitive triangle.");
			}
			if (MathUtility::Dot(first,vertices[vertexIndex].normal)<0.4999f) {
				throw std::runtime_error("Primitive normals point inward.");
			}
		}
		object.transform->translate = {-2,0,0};
		if (type == PrimitiveType::Sphere) {
			object.transform->translate.x = 2;
		}
		const Vector3 rayOrigin = MathUtility::Add(object.transform->translate,{0,0,-2});
		float distance = 0;
		if (!scene.IntersectObjectSurface(objectIndex,rayOrigin,{0,0,1},distance) || std::abs(distance-1.5f)>0.0001f) {
			throw std::runtime_error("Primitive surface picking does not match geometry.");
		}
		if (scene.GetModelPreview(*object.model,error) == 0) {
			throw std::runtime_error("Primitive thumbnail could not be created: "+error);
		}
	}
}
