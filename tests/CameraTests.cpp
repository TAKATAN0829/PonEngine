#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <fstream>
#include <stdexcept>

namespace {
	void RequireCamera(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
}

void RunCameraTests(const std::filesystem::path& directory) {
	WorldObjects scene;
	CameraView view{};
	RequireCamera(!scene.GetGameCameraView(1, view), "Empty scene has a camera");
	const WorldObjectId firstId = scene.CreateObject(WorldObjectType::Camera);
	const WorldObjectId secondId = scene.CreateObject(WorldObjectType::Camera);
	const WorldObjectId parentId = scene.CreateObject(WorldObjectType::Empty);
	RequireCamera(scene.GetMainCameraId() == firstId, "First camera was not main");
	RequireCamera(scene.GetCameraView(secondId,1,view),"Non-main camera guide unavailable");
	RequireCamera(scene.GetMainCameraId() == firstId,"Guide changed main camera");
	RequireCamera(!scene.GetCameraView(parentId,1,view),"Empty object became camera guide");
	RequireCamera(!scene.GetCameraView(999999,1,view),"Missing guide camera accepted");
	RequireCamera(scene.GetGameCameraView(1, view) && std::abs(view.cameraWorld.m[3][2]+30) < 0.001f, "Default camera view incorrect");
	const float squareProjection = view.viewProjection.m[0][0];
	RequireCamera(scene.GetGameCameraView(2,view) && std::abs(view.viewProjection.m[0][0] * 2-squareProjection) < 0.001f,
		"Camera did not follow viewport aspect ratio");
	RequireCamera(!scene.GetGameCameraView(0,view), "Zero aspect ratio accepted");
	RequireCamera(!scene.SetMainCamera(parentId) && scene.GetMainCameraId() == firstId, "Invalid camera changed main selection");
	EditorSession session;
	session.Initialize(scene);
	RequireCamera(scene.SetMainCamera(secondId) && !scene.GetEditableObject(0).mainCamera, "Main camera not exclusive");
	session.Update(scene,false);
	std::string error;
	RequireCamera(session.Undo(scene,error) && scene.GetMainCameraId() == firstId, "Main camera undo failed");
	RequireCamera(session.Redo(scene,error) && scene.GetMainCameraId() == secondId, "Main camera redo failed");
	RequireCamera(scene.SetObjectParent(secondId,parentId,error), "Cannot parent camera");
	scene.GetEditableObject(2).transform->translate.x = 5;
	scene.GetEditableObject(2).transform->rotate.y = 0.4f;
	scene.GetEditableObject(2).transform->scale = {2,3,4};
	RequireCamera(scene.GetGameCameraView(1,view), "Parented camera did not resolve");
	const float forwardLength = std::sqrt(view.cameraWorld.m[2][0]*view.cameraWorld.m[2][0]+view.cameraWorld.m[2][1]*view.cameraWorld.m[2][1]+view.cameraWorld.m[2][2]*view.cameraWorld.m[2][2]);
	RequireCamera(std::abs(forwardLength-1) < 0.001f, "Parent scale distorted camera view");
	CameraView guide{};
	RequireCamera(scene.GetCameraView(secondId,1,guide),"Parented camera guide unavailable");
	for (size_t rowIndex = 0; rowIndex < 4; ++rowIndex) {
		for (size_t columnIndex = 0; columnIndex < 4; ++columnIndex) {
			RequireCamera(std::abs(guide.viewProjection.m[rowIndex][columnIndex]-view.viewProjection.m[rowIndex][columnIndex]) < 0.0001f,
				"Guide differs from game camera projection");
		}
	}
	*scene.GetEditableObject(2).active = false;
	RequireCamera(!scene.GetGameCameraView(1,view), "Disabled parent camera remained active");
	*scene.GetEditableObject(2).active = true;
	RequireCamera(scene.DuplicateObject(secondId) != 0 && scene.GetMainCameraId() == secondId, "Duplicated camera changed main selection");
	scene.GetEditableObject(1).camera->fieldOfView = 1.0f;
	scene.GetEditableObject(1).camera->nearClip = 0.25f;
	scene.GetEditableObject(1).camera->farClip = 500;
	const std::filesystem::path cameraPath = directory / "camera.ponscene";
	RequireCamera(SceneFile::Save(scene,cameraPath,error), "Camera save failed");
	WorldObjects loaded;
	RequireCamera(SceneFile::Load(loaded,cameraPath,error), "Camera load failed");
	session.Initialize(scene);
	RequireCamera(!session.IsDirty(loaded), "Camera roundtrip lost properties");
	RequireCamera(session.BeginPlay(scene,error) && scene.SetMainCamera(0) && session.Stop(scene,error) && scene.GetMainCameraId() == secondId,
		"Play stop lost main camera");
	std::vector<WorldObjectData> invalid = loaded.CaptureObjects();
	invalid[0].mainCamera = true;
	RequireCamera(!loaded.ReplaceObjects(invalid,error), "Multiple main cameras accepted");
	invalid = loaded.CaptureObjects();
	invalid[1].camera.nearClip = invalid[1].camera.farClip;
	RequireCamera(!loaded.ReplaceObjects(invalid,error), "Invalid clip range accepted");
	invalid = loaded.CaptureObjects();
	invalid[1].camera.fieldOfView = 0;
	RequireCamera(!loaded.ReplaceObjects(invalid,error), "Zero field of view accepted");

	const std::filesystem::path legacyPath = directory / "camera-version-five.ponscene";
	std::ofstream legacyFile(legacyPath);
	legacyFile << "PON_SCENE 5\n1\n1 15 1 1 1 0 0 0 0 0 0 1 1 1 1 40 1 \"Legacy\" \"plane.gltf\" 1 1 1 0 1 10 1 0\n";
	legacyFile.close();
	RequireCamera(SceneFile::Load(loaded,legacyPath,error) && loaded.GetMainCameraId() == 0, "Version 5 camera migration failed");
}
