#include "ScenePhysics.h"
#include "SceneObjects.h"
#include "SceneFile.h"
#include "PrefabFile.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace {
	void RequirePhysics(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
	SceneObjectData Floor() {
		SceneObjectData floor;
		floor.id = 1;
		floor.name = "Floor";
		floor.collider.present = true;
		floor.collider.size = {40,1,40};
		floor.transform.translate.y = -0.5f;
		return floor;
	}
	SceneObjectData Falling(ColliderShape shape) {
		SceneObjectData object;
		object.id = 2;
		object.collider.present = true;
		object.collider.shape = shape;
		object.body.present = true;
		object.transform.translate.y = 3;
		return object;
	}
	void Advance(ScenePhysics& physics, SceneObjects& scene, size_t frames) {
		for (size_t frame = 0; frame < frames; ++frame) {
			physics.Update(scene,1.0f / 60);
		}
	}
	void VerifyLanding(ColliderShape shape) {
		SceneObjects scene;
		std::string error;
		RequirePhysics(scene.ReplaceObjects({Floor(),Falling(shape)},error), "Physics setup failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		Advance(physics,scene,240);
		RequirePhysics(std::abs(scene.GetObjectWorldPosition(2).y-0.5f) < 0.002f, "Falling object did not rest on floor");
		RequirePhysics(std::abs(physics.GetVelocity(2).y) < 0.02f, "Resting object retained downward velocity");
		RequirePhysics(scene.GetObjectWorldPosition(1).y == -0.5f, "Static floor was displaced");
		SceneCollision collision;
		collision.Update(scene);
		RequirePhysics(collision.IsTouching(2), "Resting body lost contact notification");
		physics.Finalize();
	}
	void VerifyMassAndWall() {
		SceneObjectData first = Falling(ColliderShape::Sphere);
		first.id = 1;
		first.transform.translate = {-0.25f,0,0};
		first.body.useGravity = false;
		first.body.friction = 0;
		SceneObjectData second = first;
		second.id = 2;
		second.transform.translate.x = 0.25f;
		second.body.mass = 3;
		SceneObjects scene;
		std::string error;
		RequirePhysics(scene.ReplaceObjects({first,second},error), "Mass setup failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		Advance(physics,scene,2);
		const float firstPosition = scene.GetObjectWorldPosition(1).x;
		const float secondPosition = scene.GetObjectWorldPosition(2).x;
		RequirePhysics(secondPosition-firstPosition > 0.999f, "Dynamic spheres were not separated");
		RequirePhysics(std::abs(firstPosition+3*secondPosition-0.5f) < 0.0005f, "Pushout ignored mass weighting");
		first.transform.translate.x = -0.5f;
		second.transform.translate.x = 0.5f;
		first.body.initialVelocity.x = 2;
		first.body.restitution = 1;
		RequirePhysics(scene.ReplaceObjects({first,second},error), "Dynamic impulse setup failed");
		physics.Initialize(scene);
		Advance(physics,scene,1);
		RequirePhysics(std::abs(physics.GetVelocity(1).x+1) < 0.001f && std::abs(physics.GetVelocity(2).x-1) < 0.001f, "Dynamic impulse ignored mass or momentum");
		SceneObjectData wall = Floor();
		wall.collider.size = {1,10,10};
		wall.transform.translate = {0,0,0};
		SceneObjectData mover = Falling(ColliderShape::Box);
		mover.transform.translate = {-3,0,0};
		mover.body.useGravity = false;
		mover.body.initialVelocity = {4,0,0};
		RequirePhysics(scene.ReplaceObjects({wall,mover},error), "Wall setup failed");
		physics.Initialize(scene);
		Advance(physics,scene,120);
		RequirePhysics(scene.GetObjectWorldPosition(2).x <= -0.999f && std::abs(physics.GetVelocity(2).x) < 0.01f, "Box passed through static wall");
	}
	void VerifyMaterialsAndTrigger() {
		SceneObjects scene;
		std::string error;
		SceneObjectData ball = Falling(ColliderShape::Sphere);
		ball.body.restitution = 1;
		RequirePhysics(scene.ReplaceObjects({Floor(),ball},error), "Bounce setup failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		bool bounced = false;
		for (size_t frame = 0; frame < 100; ++frame) {
			physics.Update(scene,1.0f/60);
			if (physics.GetVelocity(2).y > 2) {
				bounced = true;
			}
		}
		RequirePhysics(bounced, "Restitution did not produce bounce");
		SceneObjectData sliding = Falling(ColliderShape::Box);
		sliding.transform.translate.y = 0.5f;
		sliding.body.initialVelocity.x = 4;
		sliding.body.friction = 1;
		RequirePhysics(scene.ReplaceObjects({Floor(),sliding},error), "Friction setup failed");
		physics.Initialize(scene);
		Advance(physics,scene,120);
		RequirePhysics(std::abs(physics.GetVelocity(2).x) < 0.1f, "Contact friction did not slow sliding");
		SceneObjectData trigger = Floor();
		trigger.collider.trigger = true;
		RequirePhysics(scene.ReplaceObjects({trigger,Falling(ColliderShape::Sphere)},error), "Trigger setup failed");
		physics.Initialize(scene);
		Advance(physics,scene,120);
		RequirePhysics(scene.GetObjectWorldPosition(2).y < -2, "Trigger incorrectly pushed out falling body");
	}
	void VerifyHierarchyAndCompound() {
		SceneObjects scene;
		SceneObjectData parent;
		parent.id = 3;
		parent.type = SceneObjectType::Empty;
		parent.hasMeshRenderer = false;
		parent.transform.scale = {2,2,2};
		SceneObjectData child = Falling(ColliderShape::Box);
		child.parentId = 3;
		std::string error;
		RequirePhysics(scene.ReplaceObjects({Floor(),parent,child},error), "Scaled parent setup failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		Advance(physics,scene,240);
		RequirePhysics(std::abs(scene.GetObjectWorldPosition(2).y-1) < 0.002f, "Physics did not convert world position through parent");
		SceneObjectData root = Falling(ColliderShape::Box);
		root.collider.present = false;
		SceneObjectData firstCollider;
		firstCollider.id = 3;
		firstCollider.parentId = 2;
		firstCollider.collider.present = true;
		SceneObjectData secondCollider = firstCollider;
		secondCollider.id = 4;
		RequirePhysics(scene.ReplaceObjects({Floor(),root,firstCollider,secondCollider},error), "Compound setup failed");
		physics.Initialize(scene);
		Advance(physics,scene,240);
		RequirePhysics(std::abs(scene.GetObjectWorldPosition(2).y-0.5f) < 0.002f, "Child collider did not move owning Rigidbody");
		SceneCollision contacts;
		contacts.Update(scene);
		RequirePhysics(contacts.GetContactCount() == 2, "Colliders of the same Rigidbody collided with each other");
		std::vector<SceneObjectData> invalid = scene.CaptureObjects();
		invalid[2].body.present = true;
		RequirePhysics(!scene.ReplaceObjects(invalid,error), "Nested Dynamic bodies were accepted");
		// Inspectorで再生中に値が書き換わっても、無効な親子を解決して暴走させない。
		scene.GetEditableObject(2).body->present = true;
		const Vector3 beforeInvalid = scene.GetObjectWorldPosition(2);
		physics.Update(scene,1.0f/60);
		RequirePhysics(!physics.GetError().empty() && scene.GetObjectWorldPosition(2).y == beforeInvalid.y, "Runtime invalid hierarchy was not stopped safely");
		scene.GetEditableObject(2).body->present = false;
		physics.Update(scene,1.0f/60);
		RequirePhysics(physics.GetError().empty(), "Physics did not resume after hierarchy fix");
	}
	void VerifySettingsAndPlay(const std::filesystem::path& directory) {
		SceneObjects scene;
		std::string error;
		RequirePhysics(scene.ReplaceObjects({Floor(),Falling(ColliderShape::Sphere)},error), "Save setup failed");
		EditorSession session;
		session.Initialize(scene);
		scene.GetEditableObject(1).body->mass = 3;
		scene.GetEditableObject(1).body->initialVelocity.x = 0.25f;
		scene.GetEditableObject(0).collider->trigger = true;
		session.Update(scene,false);
		RequirePhysics(session.Undo(scene,error) && scene.GetObjectData(1)->body.mass == 1 && !scene.GetObjectData(0)->collider.trigger, "Rigidbody edit missing from undo");
		RequirePhysics(session.Redo(scene,error) && scene.GetObjectData(1)->body.mass == 3 && scene.GetObjectData(0)->collider.trigger, "Rigidbody edit missing from redo");
		const SceneBodySettings expected = scene.GetObjectData(1)->body;
		RequirePhysics(SceneFile::Save(scene,directory/"physics.ponscene",error), "Physics save failed");
		SceneObjects restored;
		RequirePhysics(SceneFile::Load(restored,directory/"physics.ponscene",error) && EqualBodySettings(restored.GetObjectData(1)->body,expected) && restored.GetObjectData(0)->collider.trigger, "Physics settings roundtrip failed");
		RequirePhysics(PrefabFile::Save(scene,2,directory/"physics.ponprefab",error), "Physics prefab save failed");
		RequirePhysics(PrefabFile::Instantiate(restored,directory/"physics.ponprefab",0,{},error) != 0 && EqualBodySettings(restored.GetObjectData(2)->body,expected), "Prefab lost Rigidbody settings");
		RequirePhysics(session.BeginPlay(scene,error), "Physics Play failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		Advance(physics,scene,120);
		RequirePhysics(scene.GetObjectWorldPosition(2).y < 1, "Physics did not advance in Play");
		session.TogglePause();
		RequirePhysics(!session.ShouldSimulate(), "Physics Pause failed");
		session.RequestStep();
		RequirePhysics(session.ShouldSimulate() && !session.ShouldSimulate(), "Physics Step failed");
		RequirePhysics(session.Stop(scene,error), "Physics Stop failed");
		physics.Finalize();
		RequirePhysics(scene.GetObjectWorldPosition(2).y == 3 && physics.GetVelocity(2).y == 0, "Stop did not restore position and clear velocity");
		std::ifstream current(directory/"physics.ponscene");
		std::ofstream legacy(directory/"physics-v10.ponscene");
		std::string line;
		std::getline(current,line);
		legacy << "PON_SCENE 10\n";
		std::getline(current,line);
		legacy << line << '\n';
		while (std::getline(current,line)) {
			size_t end = line.size();
			for (size_t field = 0; field < 13; ++field) {
				end = line.rfind(' ',end-1);
			}
			legacy << line.substr(0,end) << '\n';
		}
		legacy.close();
		RequirePhysics(SceneFile::Load(restored,directory/"physics-v10.ponscene",error) && !restored.GetObjectData(1)->body.present, "Format 10 compatibility failed");
		std::vector<SceneObjectData> invalid = scene.CaptureObjects();
		invalid[1].body.mass = 0;
		RequirePhysics(!restored.ReplaceObjects(invalid,error), "Zero mass accepted");
	}
	void VerifyKinematicAndModes() {
		SceneObjectData platform = Floor();
		platform.body.present = true;
		platform.body.mode = BodyMode::Kinematic;
		SceneObjectData rider = Falling(ColliderShape::Box);
		rider.transform.translate.y = 0.5f;
		SceneObjects scene;
		std::string error;
		RequirePhysics(scene.ReplaceObjects({platform,rider},error), "Kinematic setup failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		for (size_t frame = 0; frame < 60; ++frame) {
			scene.GetEditableObject(0).transform->translate.y += 0.01f;
			physics.Update(scene,1.0f/60);
		}
		RequirePhysics(std::abs(scene.GetObjectWorldPosition(2).y-1.1f) < 0.005f, "Kinematic platform did not lift Dynamic body");
		scene.GetEditableObject(1).body->useGravity = false;
		scene.GetEditableObject(1).body->initialVelocity = {1,0,0};
		physics.Initialize(scene);
		const float height = scene.GetObjectWorldPosition(2).y;
		Advance(physics,scene,60);
		RequirePhysics(std::abs(scene.GetObjectWorldPosition(2).x-1) < 0.001f && std::abs(scene.GetObjectWorldPosition(2).y-height) < 0.001f, "Gravity toggle or initial world velocity incorrect");
		*scene.GetEditableObject(1).active = false;
		const Vector3 stopped = scene.GetObjectWorldPosition(2);
		Advance(physics,scene,30);
		RequirePhysics(scene.GetObjectWorldPosition(2).x == stopped.x, "Inactive body kept moving");
		*scene.GetEditableObject(1).active = true;
		scene.GetEditableObject(1).body->mode = BodyMode::Static;
		Advance(physics,scene,30);
		RequirePhysics(scene.GetObjectWorldPosition(2).x == stopped.x, "Static body moved");
	}
	void VerifySlope() {
		SceneObjectData ramp = Floor();
		ramp.transform.rotate.z = 0.2f;
		ramp.body.present = true;
		ramp.body.mode = BodyMode::Static;
		ramp.body.friction = 0;
		SceneObjectData ball = Falling(ColliderShape::Sphere);
		ball.body.friction = 0;
		SceneObjects scene;
		std::string error;
		RequirePhysics(scene.ReplaceObjects({ramp,ball},error), "Slope setup failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		Advance(physics,scene,180);
		const Vector3 offset = MathUtility::Subtract(scene.GetObjectWorldPosition(2),ramp.transform.translate);
		const Vector3 normal = {-std::sin(0.2f),std::cos(0.2f),0};
		RequirePhysics(MathUtility::Dot(offset,normal) > 0.998f, "Sphere penetrated rotated ramp");
		RequirePhysics(scene.GetObjectWorldPosition(2).x < -0.2f, "Frictionless sphere did not slide down slope");
	}
	void VerifyStack() {
		SceneObjectData lower = Falling(ColliderShape::Box);
		lower.transform.translate.y = 2;
		SceneObjectData upper = Falling(ColliderShape::Box);
		upper.id = 3;
		upper.transform.translate.y = 4;
		SceneObjects scene;
		std::string error;
		RequirePhysics(scene.ReplaceObjects({Floor(),lower,upper},error), "Stack setup failed");
		ScenePhysics physics;
		physics.Initialize(scene);
		Advance(physics,scene,360);
		RequirePhysics(std::abs(scene.GetObjectWorldPosition(2).y-0.5f) < 0.005f && std::abs(scene.GetObjectWorldPosition(3).y-1.5f) < 0.005f, "Dynamic boxes did not settle in stack");
		RequirePhysics(std::abs(physics.GetVelocity(2).y) < 0.05f && std::abs(physics.GetVelocity(3).y) < 0.05f, "Stack remained unstable");
	}
}

void RunPhysicsTests(const std::filesystem::path& directory) {
	VerifyLanding(ColliderShape::Box);
	VerifyLanding(ColliderShape::Sphere);
	VerifyMassAndWall();
	VerifyMaterialsAndTrigger();
	VerifyHierarchyAndCompound();
	VerifyKinematicAndModes();
	VerifySlope();
	VerifyStack();
	VerifySettingsAndPlay(directory);
}
