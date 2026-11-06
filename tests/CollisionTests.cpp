#include "systems/WorldCollision.h"
#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include "prefab/PrefabOperations.h"
#include "../project/CG2/editor/state/EditorSession.h"
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>

namespace {
	void RequireCollision(bool isConditionSatisfied, const char* message) {
		if (!isConditionSatisfied) {
			throw std::runtime_error(message);
		}
	}
	WorldCollider MakeCollider(ColliderShape shape, const Vector3& position, const Vector3& rotation = {}, const Vector3& scale = {1,1,1}) {
		ColliderSettings settings;
		settings.shape = shape;
		WorldCollider result;
		RequireCollision(WorldCollision::BuildWorld(settings,MathUtility::MakeAffineMatrix(scale,rotation,position),result), "World collider build failed");
		return result;
	}
	void VerifyGeometry() {
		const WorldCollider box = MakeCollider(ColliderShape::Box,{});
		// 面がちょうど接する場合も、衝突として扱う境界条件を確認する。
		RequireCollision(WorldCollision::DoShapesIntersect(box,MakeCollider(ColliderShape::Box,{1,0,0})), "Touching boxes should overlap");
		RequireCollision(!WorldCollision::DoShapesIntersect(box,MakeCollider(ColliderShape::Box,{1.1f,0,0})), "Separated boxes overlap");
		RequireCollision(WorldCollision::DoShapesIntersect(box,MakeCollider(ColliderShape::Sphere,{1,0,0})), "Box sphere tangent missing");
		RequireCollision(!WorldCollision::DoShapesIntersect(box,MakeCollider(ColliderShape::Sphere,{1.01f,0,0})), "Separated box sphere overlap");
		RequireCollision(!WorldCollision::DoShapesIntersect(box,MakeCollider(ColliderShape::Sphere,{0.5009f,0.5009f,0},{},{0.002f,0.002f,0.002f})), "Small sphere corner tolerance is too large");
		RequireCollision(WorldCollision::DoShapesIntersect(box,MakeCollider(ColliderShape::Sphere,{})), "Sphere inside box missing");
		const WorldCollider sphere = MakeCollider(ColliderShape::Sphere,{});
		RequireCollision(WorldCollision::DoShapesIntersect(sphere,MakeCollider(ColliderShape::Sphere,{1,0,0})), "Sphere tangent missing");
		RequireCollision(!WorldCollision::DoShapesIntersect(sphere,MakeCollider(ColliderShape::Sphere,{1.1f,0,0})), "Separated spheres overlap");
		const WorldCollider rotated = MakeCollider(ColliderShape::Box,{}, {0,0,std::numbers::pi_v<float>/4});
		RequireCollision(!WorldCollision::DoShapesIntersect(rotated,MakeCollider(ColliderShape::Sphere,{0.64f,0.64f,0},{},{0.1f,0.1f,0.1f})), "AABB empty corner must not count as rotated box hit");
		RequireCollision(!WorldCollision::DoShapesIntersect(rotated,MakeCollider(ColliderShape::Box,{0.64f,0.64f,0},{},{0.05f,0.05f,0.05f})), "Rotated box SAT returned AABB false positive");
		RequireCollision(WorldCollision::DoShapesIntersect(rotated,MakeCollider(ColliderShape::Sphere,{0.3f,0.3f,0})), "Rotated box sphere overlap missing");
		const WorldCollider negative = MakeCollider(ColliderShape::Box,{}, {}, {-2,1,1});
		RequireCollision(WorldCollision::DoShapesIntersect(negative,MakeCollider(ColliderShape::Sphere,{1.4f,0,0})), "Negative scale box collision failed");
		WorldCollider sheared;
		Matrix4x4 shear = MathUtility::MakeIdentity4x4();
		shear.m[1][0] = 1;
		RequireCollision(WorldCollision::BuildWorld({},shear,sheared), "Sheared box build failed");
		RequireCollision(WorldCollision::DoShapesIntersect(sheared,MakeCollider(ColliderShape::Sphere,{0,0,0})), "Sheared box containment failed");
		RequireCollision(!WorldCollision::DoShapesIntersect(sheared,MakeCollider(ColliderShape::Sphere,{0.9f,-0.4f,0},{},{0.1f,0.1f,0.1f})), "Sheared box closest point incorrect");
		WorldCollider invalid;
		RequireCollision(!WorldCollision::BuildWorld({},MathUtility::MakeScaleMatrix({0,1,1}),invalid), "Degenerate box accepted");
		const WorldCollider stretchedSphere = MakeCollider(ColliderShape::Sphere,{}, {}, {2,3,1});
		RequireCollision(std::abs(stretchedSphere.radius-1.5f) < 0.0001f, "Nonuniform sphere must use largest scale");
	}
	void VerifyEvents() {
		WorldObjects scene;
		const WorldObjectId parent = scene.CreateObject(WorldObjectType::Empty);
		const WorldObjectId first = scene.CreateObject(WorldObjectType::Empty);
		const WorldObjectId second = scene.CreateObject(WorldObjectType::Empty);
		std::string error;
		RequireCollision(scene.SetObjectParent(first,parent,error), "Collider parenting failed");
		scene.GetEditableObject(1).collider->isPresent = true;
		scene.GetEditableObject(2).collider->isPresent = true;
		scene.GetEditableObject(2).collider->shape = ColliderShape::Sphere;
		WorldCollision collision;
		collision.Initialize();
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && collision.GetEvents()[0].isEntering && collision.IsTouching(first), "Collision Enter missing");
		collision.Update(scene);
		RequireCollision(collision.GetEvents().empty() && collision.GetContactCount() == 1, "Stay spammed events");
		*scene.GetEditableObject(0).isActive = false;
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && !collision.GetEvents()[0].isEntering && !collision.IsTouching(first), "Inactive hierarchy did not exit");
		*scene.GetEditableObject(0).isActive = true;
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && collision.GetEvents()[0].isEntering, "Reenabled collision did not enter");
		scene.GetEditableObject(2).collider->isEnabled = false;
		collision.Update(scene);
		RequireCollision(collision.GetContactCount() == 0 && collision.GetEvents().size() == 1, "Disabled collider retained contacts");
		scene.GetEditableObject(2).collider->isEnabled = true;
		collision.Update(scene);
		RequireCollision(scene.RemoveObject(second), "Collider deletion failed");
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && !collision.GetEvents()[0].isEntering, "Deleted collider did not exit");
		collision.Initialize();
		RequireCollision(collision.GetEvents().empty() && collision.GetContactCount() == 0, "Play reset retained old contacts");
	}
	void VerifyPersistence(const std::filesystem::path& directory) {
		WorldObjects scene;
		const WorldObjectId identifier = scene.AddObject("plane.gltf");
		EditorSession history;
		history.Initialize(scene);
		scene.GetEditableObject(0).collider->isPresent = true;
		scene.GetEditableObject(0).collider->center = {1,2,3};
		scene.GetEditableObject(0).collider->size = {4,5,6};
		scene.GetEditableObject(0).collider->radius = 7;
		const ColliderSettings expected = *scene.GetEditableObject(0).collider;
		history.Update(scene,false);
		std::string error;
		RequireCollision(history.Undo(scene,error) && !scene.GetObjectData(0)->collider.isPresent, "Collider add missing from undo");
		RequireCollision(history.Redo(scene,error) && AreColliderSettingsEqual(scene.GetObjectData(0)->collider,expected), "Collider settings missing from redo");
		const std::filesystem::path file = directory / "collider.ponscene";
		RequireCollision(SceneFile::Save(scene,file,error), "Collider save failed");
		WorldObjects restored;
		RequireCollision(SceneFile::Load(restored,file,error) && AreColliderSettingsEqual(restored.GetObjectData(0)->collider,expected), "Collider roundtrip failed");
		RequireCollision(PrefabOperations::Save(scene,identifier,directory/"collider.ponprefab",error), "Collider prefab save failed");
		RequireCollision(PrefabOperations::Instantiate(restored,directory/"collider.ponprefab",0,{},error) != 0, "Collider prefab placement failed");
		RequireCollision(AreColliderSettingsEqual(restored.GetObjectData(1)->collider,expected), "Prefab lost collider settings");
		std::vector<WorldObjectData> invalid = scene.CaptureObjects();
		invalid[0].collider.radius = 0;
		RequireCollision(!restored.ReplaceObjects(invalid,error), "Zero radius accepted");
		invalid[0].collider = expected;
		invalid[0].collider.shape = static_cast<ColliderShape>(99);
		RequireCollision(!restored.ReplaceObjects(invalid,error), "Invalid collider type accepted");
		std::ifstream current(file);
		std::ofstream legacy(directory/"collider-v9.ponscene");
		std::string line;
		std::getline(current,line);
		legacy << "PON_SCENE 9\n";
		std::getline(current,line);
		legacy << line << '\n';
		std::getline(current,line);
		size_t end = line.size();
		// Collider10項目＋Body13項目＋Light角度2項目を除き、形式9へ戻す。
		for (size_t field = 0; field < 25; ++field) {
			end = line.rfind(' ',end-1);
		}
		legacy << line.substr(0,end) << '\n';
		legacy.close();
		RequireCollision(SceneFile::Load(restored,directory/"collider-v9.ponscene",error) && !restored.GetObjectData(0)->collider.isPresent, "Format 9 compatibility failed");
		std::ofstream broken(directory/"collider-invalid.ponscene");
		broken << "PON_SCENE 10\n1\n" << line.substr(0,end) << " 1 1 99 0 0 0 1 1 1 0.5\n";
		broken.close();
		RequireCollision(!SceneFile::Load(restored,directory/"collider-invalid.ponscene",error), "Invalid serialized collider type accepted");
		RequireCollision(restored.GetEditableObjectCount() == 1 && !restored.GetObjectData(0)->collider.isPresent, "Failed collider load partially changed scene");
		std::vector<WorldObjectData> tooMany;
		for (size_t colliderIndex = 0; colliderIndex <= WorldCollision::kMaxColliders; ++colliderIndex) {
			WorldObjectData object;
			object.id = colliderIndex+1;
			object.collider.isPresent = true;
			tooMany.push_back(object);
		}
		RequireCollision(!restored.ReplaceObjects(tooMany,error), "Collider limit not enforced");
	}
}

void RunCollisionTests(const std::filesystem::path& directory) {
	VerifyGeometry();
	VerifyEvents();
	VerifyPersistence(directory);
}
