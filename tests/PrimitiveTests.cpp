#include "objects/WorldPrimitives.h"
#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include "serialization/PrefabFile.h"
#include "../project/CG2/editor/EditorSession.h"
#include <stdexcept>

namespace {
	void RequirePrimitive(bool isConditionSatisfied, const char* message) {
		if (!isConditionSatisfied) {
			throw std::runtime_error(message);
		}
	}
}

void RunPrimitiveTests(const std::filesystem::path& directory) {
	WorldObjects scene;
	EditorSession history;
	history.Initialize(scene);
	std::string error;
	const WorldObjectId cube = WorldPrimitives::Create(scene,PrimitiveType::Cube,error);
	RequirePrimitive(cube != 0,"Cube creation failed");
	history.Update(scene,false);
	const WorldObjectId sphere = WorldPrimitives::Create(scene,PrimitiveType::Sphere,error);
	RequirePrimitive(sphere != 0,"Sphere creation failed");
	history.Update(scene,false);
	RequirePrimitive(scene.GetObjectData(0)->model == WorldPrimitives::kCubeModel && scene.GetObjectData(1)->model == WorldPrimitives::kSphereModel,"Primitive model paths incorrect");
	RequirePrimitive(scene.GetObjectData(0)->collider.isPresent && scene.GetObjectData(0)->collider.shape == ColliderShape::Box,"Cube collider incorrect");
	RequirePrimitive(scene.GetObjectData(1)->collider.isPresent && scene.GetObjectData(1)->collider.shape == ColliderShape::Sphere && scene.GetObjectData(1)->collider.radius == 0.5f,"Sphere collider incorrect");
	RequirePrimitive(history.Undo(scene,error) && scene.GetEditableObjectCount() == 1,"Primitive creation was not one undo action");
	RequirePrimitive(history.Redo(scene,error) && scene.GetObjectData(1)->id == sphere,"Primitive redo lost identity");
	scene.GetEditableObject(1).transform->translate = {0,2,0};
	RequirePrimitive(scene.SetObjectParent(sphere,cube,error),"Primitive parenting failed");
	const std::filesystem::path saved = directory/"primitives.ponscene";
	RequirePrimitive(SceneFile::Save(scene,saved,error),"Primitive save failed");
	WorldObjects loaded;
	RequirePrimitive(SceneFile::Load(loaded,saved,error),"Primitive reload failed");
	RequirePrimitive(loaded.GetObjectData(1)->parentId == cube && loaded.GetObjectWorldPosition(sphere).y == 2,"Primitive hierarchy lost on reload");
	RequirePrimitive(loaded.GetObjectData(0)->collider.isPresent && loaded.GetObjectData(1)->model == WorldPrimitives::kSphereModel,"Primitive properties lost on reload");
	const std::filesystem::path prefab = directory/"primitives.ponprefab";
	RequirePrimitive(PrefabFile::Save(scene,cube,prefab,error),"Primitive prefab save failed");
	RequirePrimitive(PrefabFile::Instantiate(loaded,prefab,0,{},error) != 0 && loaded.GetEditableObjectCount() == 4,"Primitive prefab import failed");
	RequirePrimitive(loaded.GetObjectData(3)->collider.shape == ColliderShape::Sphere,"Primitive prefab lost collider");
	const size_t originalCount = loaded.GetEditableObjectCount();
	RequirePrimitive(WorldPrimitives::Create(loaded,static_cast<PrimitiveType>(99),error) == 0 && loaded.GetEditableObjectCount() == originalCount,"Invalid primitive partially created an object");
	std::vector<WorldObjectData> full;
	for (size_t objectIndex = 0; objectIndex < kMaxWorldColliders; ++objectIndex) {
		WorldObjectData object;
		object.id = objectIndex+1;
		object.collider.isPresent = true;
		full.push_back(object);
	}
	RequirePrimitive(loaded.ReplaceObjects(full,error),"Primitive collider limit fixture failed");
	RequirePrimitive(WorldPrimitives::Create(loaded,PrimitiveType::Cube,error) == 0 && loaded.GetEditableObjectCount() == kMaxWorldColliders,"Collider limit left partially created primitive");
	RequirePrimitive(loaded.RemoveObject(1),"Cannot release collider slot");
	RequirePrimitive(WorldPrimitives::Create(loaded,PrimitiveType::Sphere,error) > kMaxWorldColliders,"Primitive creation reused retired ID");
}
