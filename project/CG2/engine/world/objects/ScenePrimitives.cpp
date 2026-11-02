#include "objects/ScenePrimitives.h"
#include <limits>
#include <utility>

SceneObjectId ScenePrimitives::Create(ISceneObjects& scene, ScenePrimitiveType type, std::string& error) {
	error.clear();
	if (!scene.SupportsObjectCollection()) {
		error = "This scene does not support primitive creation.";
		return kInvalidSceneObjectId;
	}
	if (type != ScenePrimitiveType::Cube && type != ScenePrimitiveType::Sphere) {
		error = "Unknown primitive type.";
		return kInvalidSceneObjectId;
	}
	SceneObjectData primitive;
	primitive.id = scene.GetNextObjectId();
	if (primitive.id == kInvalidSceneObjectId || primitive.id == (std::numeric_limits<SceneObjectId>::max)()) {
		error = "No object IDs available.";
		return kInvalidSceneObjectId;
	}
	primitive.name = "Cube #"+std::to_string(primitive.id);
	primitive.model = kCubeModel;
	primitive.collider.present = true;
	if (type == ScenePrimitiveType::Sphere) {
		primitive.name = "Sphere #"+std::to_string(primitive.id);
		primitive.model = kSphereModel;
		primitive.collider.shape = ColliderShape::Sphere;
	}
	// 作成操作時だけ値を複製し、モデル読込とCollider上限も含めて一括検証する。
	std::vector<SceneObjectData> objects = scene.CaptureObjects();
	objects.push_back(primitive);
	if (!scene.ReplaceObjects(std::move(objects),error)) {
		return kInvalidSceneObjectId;
	}
	return primitive.id;
}
