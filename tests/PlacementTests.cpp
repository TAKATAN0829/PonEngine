#include "SceneObjects.h"
#include "SceneViewMath.h"
#include "../project/CG2/editor/AssetDragPayload.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <stdexcept>
#include <limits>

namespace {
	void CheckPlacement(bool condition) {
		if (!condition) {
			throw std::runtime_error("Model placement regression");
		}
	}
}
void RunPlacementTests() {
	SceneObjects scene;
	EditorSession session;
	session.Initialize(scene);
	std::string error;
	const SceneObjectId placed = scene.PlaceModel("plane.gltf",0,{3,4,5},error);
	CheckPlacement(placed != 0 && scene.GetObjectWorldPosition(placed).x == 3);
	session.Update(scene,false);
	CheckPlacement(session.Undo(scene,error) && scene.GetEditableObjectCount() == 0);
	CheckPlacement(session.Redo(scene,error) && scene.GetEditableObjectCount() == 1);
	const SceneObjectId parent = scene.CreateObject(SceneObjectType::Empty);
	scene.GetEditableObject(1).transform->translate = {10,2,3};
	scene.GetEditableObject(1).transform->rotate.y = 0.4f;
	scene.GetEditableObject(1).transform->scale = {2,3,4};
	const SceneObjectId child = scene.PlaceModel("axis.obj",parent,{10,2,3},error);
	CheckPlacement(child != 0 && scene.GetEditableObject(2).parentId == parent);
	CheckPlacement(std::abs(scene.GetObjectWorldPosition(child).x-10) < 0.001f);
	const size_t count = scene.GetEditableObjectCount();
	CheckPlacement(scene.PlaceModel("missing.obj",0,{},error) == 0 && scene.GetEditableObjectCount() == count);
	CheckPlacement(scene.PlaceModel("axis.obj",999,{},error) == 0 && scene.GetEditableObjectCount() == count);
	CheckPlacement(scene.PlaceModel("axis.obj",0,{std::numeric_limits<float>::infinity(),0,0},error) == 0 && scene.GetEditableObjectCount() == count);
	scene.GetEditableObject(1).transform->scale = {0,1,1};
	CheckPlacement(scene.PlaceModel("axis.obj",parent,{},error) == 0 && scene.GetEditableObjectCount() == count);
	CheckPlacement(SceneViewMath::GetPlacementPosition({0,5,0},{0,-1,0},100,101).y == 0);
	CheckPlacement(SceneViewMath::GetPlacementPosition({0,5,0},{0,-1,0},100,2).y == 3);
	CheckPlacement(SceneViewMath::GetPlacementPosition({0,0,0},{0,0,1},100,101).z == 10);
	CheckPlacement(SceneViewMath::GetPlacementPosition({0,0,0},{0,0,1},5,6).z == 5);
	ModelAssetPayload payload;
	std::memcpy(payload.model,"plane.gltf",11);
	std::string model;
	CheckPlacement(ModelAssetPayload::Read(&payload,sizeof(payload),model) && model == "plane.gltf");
	CheckPlacement(!ModelAssetPayload::Read(&payload,3,model));
	std::memset(payload.model,'a',sizeof(payload.model));
	CheckPlacement(!ModelAssetPayload::Read(&payload,sizeof(payload),model));
}
