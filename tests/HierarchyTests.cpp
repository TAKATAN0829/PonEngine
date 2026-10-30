#include "SceneObjects.h"
#include "SceneFile.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <fstream>
#include <stdexcept>
#include <utility>

namespace {
	void RequireHierarchy(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	// 位置だけでなく回転・拡縮も比較して、親子付けで見た目が変わらないことを確認する。
	void RequireMatrixNear(const Matrix4x4& first, const Matrix4x4& second) {
		for (size_t rowIndex = 0; rowIndex < 4; ++rowIndex) {
			for (size_t columnIndex = 0; columnIndex < 4; ++columnIndex) {
				RequireHierarchy(std::abs(first.m[rowIndex][columnIndex] - second.m[rowIndex][columnIndex]) < 0.001f, "World matrix changed unexpectedly");
			}
		}
	}
}

void RunHierarchyTests(const std::filesystem::path& directory) {
	SceneObjects scene;
	const SceneObjectId parentId = scene.CreateObject(SceneObjectType::Empty);
	const SceneObjectId childId = scene.AddObject("plane.gltf");
	const SceneObjectId lightId = scene.CreateObject(SceneObjectType::PointLight);
	*scene.GetEditableObject(0).transform = {{2,2,2},{0.2f,0.4f,-0.1f},{10,3,-2}};
	*scene.GetEditableObject(1).transform = {{1,2,3},{0.3f,-0.5f,0.2f},{3,4,5}};
	Matrix4x4 originalWorld{};
	Matrix4x4 changedWorld{};
	RequireHierarchy(scene.GetObjectWorldMatrix(childId, originalWorld), "Cannot resolve root transform");
	EditorSession session;
	session.Initialize(scene);
	std::string error;
	RequireHierarchy(scene.SetObjectParent(childId, parentId, error), "Cannot parent child");
	RequireHierarchy(scene.GetObjectWorldMatrix(childId, changedWorld), "Cannot resolve child transform");
	RequireMatrixNear(originalWorld, changedWorld);
	session.Update(scene, false);
	RequireHierarchy(session.Undo(scene, error) && scene.GetEditableObject(1).parentId == 0, "Parenting undo failed");
	RequireHierarchy(session.Redo(scene, error) && scene.GetEditableObject(1).parentId == parentId, "Parenting redo failed");
	RequireHierarchy(!scene.SetObjectParent(parentId, childId, error), "Parent cycle accepted");
	RequireHierarchy(!scene.SetObjectParent(childId, childId, error), "Self parent accepted");
	RequireHierarchy(!scene.SetObjectParent(childId, 999, error), "Missing parent accepted");
	RequireHierarchy(scene.SetObjectWorldPosition(childId, {7,8,9}), "Cannot move child in world space");
	const Vector3 movedPosition = scene.GetObjectWorldPosition(childId);
	RequireHierarchy(std::abs(movedPosition.x-7) < 0.001f && std::abs(movedPosition.y-8) < 0.001f && std::abs(movedPosition.z-9) < 0.001f,
		"World gizmo used local coordinates");
	RequireHierarchy(scene.SetObjectParent(lightId, parentId, error), "Cannot parent light");
	scene.GetEditableObject(0).transform->translate.x += 5;
	RequireHierarchy(std::abs(scene.GetObjectWorldPosition(childId).x-12) < 0.001f, "Child did not follow parent");
	*scene.GetEditableObject(0).active = false;
	RequireHierarchy(!scene.IsObjectActive(childId) && !scene.IsObjectActive(lightId), "Inactive parent did not hide descendants");
	*scene.GetEditableObject(0).active = true;
	RequireHierarchy(scene.IsObjectActive(childId), "Active child did not recover");

	const SceneObjectId copiedId = scene.DuplicateObject(parentId);
	RequireHierarchy(copiedId != 0 && scene.GetEditableObjectCount() == 6, "Subtree duplicate failed");
	RequireHierarchy(scene.GetEditableObject(4).parentId == copiedId && scene.GetEditableObject(5).parentId == copiedId,
		"Copied children reference original parent");
	RequireHierarchy(scene.GetObjectWorldMatrix(childId, originalWorld) && scene.GetObjectWorldMatrix(scene.GetEditableObject(4).id, changedWorld), "Copied world transform missing");
	RequireMatrixNear(originalWorld, changedWorld);
	const std::filesystem::path hierarchyPath = directory / "hierarchy.ponscene";
	RequireHierarchy(SceneFile::Save(scene, hierarchyPath, error), "Hierarchy save failed");
	SceneObjects loaded;
	RequireHierarchy(SceneFile::Load(loaded, hierarchyPath, error), "Hierarchy load failed");
	session.Initialize(scene);
	RequireHierarchy(!session.IsDirty(loaded), "Hierarchy roundtrip lost data");
	RequireHierarchy(scene.RemoveObject(parentId) && scene.GetEditableObjectCount() == 3, "Subtree deletion failed");
	RequireHierarchy(scene.GetObjectWorldMatrix(copiedId, changedWorld), "Deletion broke ID lookup");
	RequireHierarchy(scene.AddObject("axis.obj") > copiedId, "ID reused after deletion");

	// 親参照の検証で失敗しても、現在の一覧を部分的に変更しない。
	std::vector<SceneObjectData> invalid = loaded.CaptureObjects();
	invalid[0].parentId = 999;
	RequireHierarchy(!loaded.ReplaceObjects(invalid, error) && loaded.GetEditableObject(0).parentId == 0, "Orphan parent partially applied");
	invalid = loaded.CaptureObjects();
	invalid[0].parentId = childId;
	RequireHierarchy(!loaded.ReplaceObjects(invalid, error), "Serialized cycle accepted");
	invalid = loaded.CaptureObjects();
	std::swap(invalid[0], invalid[1]);
	RequireHierarchy(loaded.ReplaceObjects(invalid, error), "Child-before-parent ordering rejected");

	SceneObjects scaled;
	scaled.CreateObject(SceneObjectType::Empty);
	scaled.AddObject("plane.gltf");
	scaled.GetEditableObject(0).transform->scale = {0,1,1};
	RequireHierarchy(!scaled.SetObjectParent(2,1,error), "Zero scale parent accepted");
	scaled.GetEditableObject(0).transform->scale = {2,1,1};
	scaled.GetEditableObject(1).transform->rotate.y = 0.7f;
	RequireHierarchy(!scaled.SetObjectParent(2,1,error) && scaled.GetEditableObject(1).parentId == 0, "Sheared reparent approximated silently");
	scaled.GetEditableObject(0).transform->scale = {-2,2,2};
	RequireHierarchy(scaled.GetObjectWorldMatrix(2,originalWorld) && scaled.SetObjectParent(2,1,error), "Negative scale reparent failed");
	RequireHierarchy(scaled.GetObjectWorldMatrix(2,changedWorld), "Negative scale world missing");
	RequireMatrixNear(originalWorld,changedWorld);
	RequireHierarchy(scaled.SetObjectParent(2,0,error), "Cannot unparent object");
	scaled.GetEditableObject(0).transform->scale = {1,1,1};
	scaled.GetEditableObject(1).transform->rotate = {0.3f, 1.57079632679f, 0.4f};
	RequireHierarchy(scaled.GetObjectWorldMatrix(2,originalWorld) && scaled.SetObjectParent(2,1,error), "Gimbal-angle parenting failed");
	RequireHierarchy(scaled.GetObjectWorldMatrix(2,changedWorld), "Gimbal-angle world missing");
	RequireMatrixNear(originalWorld,changedWorld);

	// 三段のツリーでも、子孫の親IDを複製先へ付け直してUndoでまとめて復元する。
	SceneObjects nested;
	nested.CreateObject(SceneObjectType::Empty);
	nested.CreateObject(SceneObjectType::Empty);
	nested.AddObject("axis.obj");
	RequireHierarchy(nested.SetObjectParent(2,1,error) && nested.SetObjectParent(3,2,error), "Three-level tree failed");
	nested.GetEditableObject(0).transform->translate.x = 5;
	nested.GetEditableObject(1).transform->translate.x = 2;
	RequireHierarchy(std::abs(nested.GetObjectWorldPosition(3).x-7) < 0.001f, "Grandchild did not follow ancestors");
	RequireHierarchy(nested.DuplicateObject(1) == 4 && nested.GetEditableObject(5).parentId == 5, "Grandchild clone parent incorrect");
	session.Initialize(nested);
	RequireHierarchy(session.BeginPlay(nested,error), "Cannot play hierarchy");
	RequireHierarchy(nested.SetObjectParent(3,0,error), "Cannot reparent during play");
	RequireHierarchy(session.Stop(nested,error) && nested.GetEditableObject(2).parentId == 2, "Stop lost edit-mode parent");
	RequireHierarchy(nested.RemoveObject(1), "Cannot delete nested root");
	session.Update(nested,false);
	RequireHierarchy(nested.GetEditableObjectCount() == 3 && session.Undo(nested,error) && nested.GetEditableObjectCount() == 6,
		"Subtree delete undo failed");

	std::vector<SceneObjectData> deepObjects;
	for (SceneObjectId objectId = 1; objectId <= 65; ++objectId) {
		SceneObjectData object;
		object.id = objectId;
		object.parentId = objectId - 1;
		deepObjects.push_back(object);
	}
	RequireHierarchy(!nested.ReplaceObjects(deepObjects,error), "Over-deep hierarchy accepted");
	deepObjects.pop_back();
	RequireHierarchy(nested.ReplaceObjects(deepObjects,error) && nested.GetObjectWorldMatrix(64,changedWorld), "64-level hierarchy rejected");

	// 形式4のレコードには親IDがなく、従来どおり最上位へ復元する。
	const std::filesystem::path legacyPath = directory / "hierarchy-version-four.ponscene";
	std::ofstream legacyFile(legacyPath);
	legacyFile << "PON_SCENE 4\n1\n1 15 1 1 1 0 0 0 1 2 3 1 1 1 1 40 1 \"Legacy\" \"plane.gltf\" 1 1 1 0 1 10 1\n";
	legacyFile.close();
	RequireHierarchy(SceneFile::Load(loaded, legacyPath, error) && loaded.GetEditableObject(0).parentId == 0, "Version 4 migration failed");
}
