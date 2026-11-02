#include "objects/WorldPrimitives.h"
#include <limits>
#include <utility>

WorldObjectId WorldPrimitives::Create(IWorldObjects& scene, PrimitiveType type, std::string& error) {
	error.clear();
	if (!scene.SupportsObjectCollection()) {
		error = "This scene does not support primitive creation.";
		return kInvalidWorldObjectId;
	}
	if (type != PrimitiveType::Cube && type != PrimitiveType::Sphere) {
		error = "Unknown primitive type.";
		return kInvalidWorldObjectId;
	}
	WorldObjectData primitive;
	primitive.id = scene.GetNextObjectId();
	if (primitive.id == kInvalidWorldObjectId || primitive.id == (std::numeric_limits<WorldObjectId>::max)()) {
		error = "No object IDs available.";
		return kInvalidWorldObjectId;
	}
	primitive.name = "Cube #"+std::to_string(primitive.id);
	primitive.model = kCubeModel;
	primitive.collider.present = true;
	if (type == PrimitiveType::Sphere) {
		primitive.name = "Sphere #"+std::to_string(primitive.id);
		primitive.model = kSphereModel;
		primitive.collider.shape = ColliderShape::Sphere;
	}
	// 作成操作時だけ値を複製し、モデル読込とCollider上限も含めて一括検証する。
	std::vector<WorldObjectData> objects = scene.CaptureObjects();
	objects.push_back(primitive);
	if (!scene.ReplaceObjects(std::move(objects),error)) {
		return kInvalidWorldObjectId;
	}
	return primitive.id;
}
