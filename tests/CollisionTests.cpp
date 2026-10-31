#include "SceneCollision.h"
#include "SceneObjects.h"
#include "SceneFile.h"
#include "PrefabFile.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <fstream>
#include <numbers>
#include <stdexcept>

namespace {
	void RequireCollision(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
	WorldCollider MakeCollider(ColliderShape shape, const Vector3& position, const Vector3& rotation = {}, const Vector3& scale = {1,1,1}) {
		SceneColliderSettings settings;
		settings.shape = shape;
		WorldCollider result;
		RequireCollision(SceneCollision::BuildWorld(settings,MathUtility::MakeAffineMatrix(scale,rotation,position),result), "World collider build failed");
		return result;
	}
	void VerifyGeometry() {
		const WorldCollider box = MakeCollider(ColliderShape::Box,{});
		RequireCollision(SceneCollision::Intersects(box,MakeCollider(ColliderShape::Box,{1,0,0})), "Touching boxes should overlap");
		RequireCollision(!SceneCollision::Intersects(box,MakeCollider(ColliderShape::Box,{1.1f,0,0})), "Separated boxes overlap");
		RequireCollision(SceneCollision::Intersects(box,MakeCollider(ColliderShape::Sphere,{1,0,0})), "Box sphere tangent missing");
		RequireCollision(!SceneCollision::Intersects(box,MakeCollider(ColliderShape::Sphere,{1.01f,0,0})), "Separated box sphere overlap");
		RequireCollision(!SceneCollision::Intersects(box,MakeCollider(ColliderShape::Sphere,{0.5009f,0.5009f,0},{},{0.002f,0.002f,0.002f})), "Small sphere corner tolerance is too large");
		RequireCollision(SceneCollision::Intersects(box,MakeCollider(ColliderShape::Sphere,{})), "Sphere inside box missing");
		const WorldCollider sphere = MakeCollider(ColliderShape::Sphere,{});
		RequireCollision(SceneCollision::Intersects(sphere,MakeCollider(ColliderShape::Sphere,{1,0,0})), "Sphere tangent missing");
		RequireCollision(!SceneCollision::Intersects(sphere,MakeCollider(ColliderShape::Sphere,{1.1f,0,0})), "Separated spheres overlap");
		const WorldCollider rotated = MakeCollider(ColliderShape::Box,{}, {0,0,std::numbers::pi_v<float>/4});
		RequireCollision(!SceneCollision::Intersects(rotated,MakeCollider(ColliderShape::Sphere,{0.64f,0.64f,0},{},{0.1f,0.1f,0.1f})), "AABB empty corner must not count as rotated box hit");
		RequireCollision(!SceneCollision::Intersects(rotated,MakeCollider(ColliderShape::Box,{0.64f,0.64f,0},{},{0.05f,0.05f,0.05f})), "Rotated box SAT returned AABB false positive");
		RequireCollision(SceneCollision::Intersects(rotated,MakeCollider(ColliderShape::Sphere,{0.3f,0.3f,0})), "Rotated box sphere overlap missing");
		const WorldCollider negative = MakeCollider(ColliderShape::Box,{}, {}, {-2,1,1});
		RequireCollision(SceneCollision::Intersects(negative,MakeCollider(ColliderShape::Sphere,{1.4f,0,0})), "Negative scale box collision failed");
		WorldCollider sheared;
		Matrix4x4 shear = MathUtility::MakeIdentity4x4();
		shear.m[1][0] = 1;
		RequireCollision(SceneCollision::BuildWorld({},shear,sheared), "Sheared box build failed");
		RequireCollision(SceneCollision::Intersects(sheared,MakeCollider(ColliderShape::Sphere,{0,0,0})), "Sheared box containment failed");
		RequireCollision(!SceneCollision::Intersects(sheared,MakeCollider(ColliderShape::Sphere,{0.9f,-0.4f,0},{},{0.1f,0.1f,0.1f})), "Sheared box closest point incorrect");
		WorldCollider invalid;
		RequireCollision(!SceneCollision::BuildWorld({},MathUtility::MakeScaleMatrix({0,1,1}),invalid), "Degenerate box accepted");
		const WorldCollider stretchedSphere = MakeCollider(ColliderShape::Sphere,{}, {}, {2,3,1});
		RequireCollision(std::abs(stretchedSphere.radius-1.5f) < 0.0001f, "Nonuniform sphere must use largest scale");
	}
	void VerifyEvents() {
		SceneObjects scene;
		const SceneObjectId parent = scene.CreateObject(SceneObjectType::Empty);
		const SceneObjectId first = scene.CreateObject(SceneObjectType::Empty);
		const SceneObjectId second = scene.CreateObject(SceneObjectType::Empty);
		std::string error;
		RequireCollision(scene.SetObjectParent(first,parent,error), "Collider parenting failed");
		scene.GetEditableObject(1).collider->present = true;
		scene.GetEditableObject(2).collider->present = true;
		scene.GetEditableObject(2).collider->shape = ColliderShape::Sphere;
		SceneCollision collision;
		collision.Initialize();
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && collision.GetEvents()[0].entering && collision.IsTouching(first), "Collision Enter missing");
		collision.Update(scene);
		RequireCollision(collision.GetEvents().empty() && collision.GetContactCount() == 1, "Stay spammed events");
		*scene.GetEditableObject(0).active = false;
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && !collision.GetEvents()[0].entering && !collision.IsTouching(first), "Inactive hierarchy did not exit");
		*scene.GetEditableObject(0).active = true;
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && collision.GetEvents()[0].entering, "Reenabled collision did not enter");
		scene.GetEditableObject(2).collider->enabled = false;
		collision.Update(scene);
		RequireCollision(collision.GetContactCount() == 0 && collision.GetEvents().size() == 1, "Disabled collider retained contacts");
		scene.GetEditableObject(2).collider->enabled = true;
		collision.Update(scene);
		RequireCollision(scene.RemoveObject(second), "Collider deletion failed");
		collision.Update(scene);
		RequireCollision(collision.GetEvents().size() == 1 && !collision.GetEvents()[0].entering, "Deleted collider did not exit");
		collision.Initialize();
		RequireCollision(collision.GetEvents().empty() && collision.GetContactCount() == 0, "Play reset retained old contacts");
	}
	void VerifyPersistence(const std::filesystem::path& directory) {
		SceneObjects scene;
		const SceneObjectId identifier = scene.AddObject("plane.gltf");
		EditorSession history;
		history.Initialize(scene);
		scene.GetEditableObject(0).collider->present = true;
		scene.GetEditableObject(0).collider->center = {1,2,3};
		scene.GetEditableObject(0).collider->size = {4,5,6};
		scene.GetEditableObject(0).collider->radius = 7;
		const SceneColliderSettings expected = *scene.GetEditableObject(0).collider;
		history.Update(scene,false);
		std::string error;
		RequireCollision(history.Undo(scene,error) && !scene.GetObjectData(0)->collider.present, "Collider add missing from undo");
		RequireCollision(history.Redo(scene,error) && EqualColliderSettings(scene.GetObjectData(0)->collider,expected), "Collider settings missing from redo");
		const std::filesystem::path file = directory / "collider.ponscene";
		RequireCollision(SceneFile::Save(scene,file,error), "Collider save failed");
		SceneObjects restored;
		RequireCollision(SceneFile::Load(restored,file,error) && EqualColliderSettings(restored.GetObjectData(0)->collider,expected), "Collider roundtrip failed");
		RequireCollision(PrefabFile::Save(scene,identifier,directory/"collider.ponprefab",error), "Collider prefab save failed");
		RequireCollision(PrefabFile::Instantiate(restored,directory/"collider.ponprefab",0,{},error) != 0, "Collider prefab placement failed");
		RequireCollision(EqualColliderSettings(restored.GetObjectData(1)->collider,expected), "Prefab lost collider settings");
		std::vector<SceneObjectData> invalid = scene.CaptureObjects();
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
		for (size_t field = 0; field < 23; ++field) {
			end = line.rfind(' ',end-1);
		}
		legacy << line.substr(0,end) << '\n';
		legacy.close();
		RequireCollision(SceneFile::Load(restored,directory/"collider-v9.ponscene",error) && !restored.GetObjectData(0)->collider.present, "Format 9 compatibility failed");
		std::ofstream broken(directory/"collider-invalid.ponscene");
		broken << "PON_SCENE 10\n1\n" << line.substr(0,end) << " 1 1 99 0 0 0 1 1 1 0.5\n";
		broken.close();
		RequireCollision(!SceneFile::Load(restored,directory/"collider-invalid.ponscene",error), "Invalid serialized collider type accepted");
		RequireCollision(restored.GetEditableObjectCount() == 1 && !restored.GetObjectData(0)->collider.present, "Failed collider load partially changed scene");
		std::vector<SceneObjectData> tooMany;
		for (size_t colliderIndex = 0; colliderIndex <= SceneCollision::kMaxColliders; ++colliderIndex) {
			SceneObjectData object;
			object.id = colliderIndex+1;
			object.collider.present = true;
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
