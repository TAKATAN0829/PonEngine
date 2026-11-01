#include "SceneLighting.h"
#include "SceneObjects.h"
#include "SceneFile.h"
#include "PrefabFile.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace {
	void RequireLight(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
	// 新しいモデルデータから旧Scene12のレコードを作り、本番の移行経路へ渡す。
	void WriteLegacyScene(SceneObjects& source, const std::filesystem::path& path) {
		std::string error;
		RequireLight(SceneFile::Save(source,path,error),"Legacy lighting fixture save failed");
		std::ifstream input(path);
		std::ostringstream converted;
		std::string line;
		std::getline(input,line);
		converted << "PON_SCENE 12\n";
		while (std::getline(input,line)) {
			const std::string suffix = " 60 0";
			if (line.ends_with(suffix)) {
				line.erase(line.size()-suffix.size());
			}
			converted << line << '\n';
		}
		input.close();
		std::ofstream output(path);
		output << converted.str();
	}
}

void RunLightingTests(const std::filesystem::path& directory) {
	SceneObjects scene;
	const SceneObjectId root = scene.CreateObject(SceneObjectType::Empty);
	const SceneObjectId point = scene.CreateObject(SceneObjectType::PointLight);
	const SceneObjectId spot = scene.CreateObject(SceneObjectType::SpotLight);
	const SceneObjectId directional = scene.CreateObject(SceneObjectType::DirectionalLight);
	RequireLight(root != 0 && point != 0 && spot != 0 && directional != 0,"Cannot create all light types");
	std::string error;
	RequireLight(scene.SetObjectParent(point,root,error),"Cannot parent point light");
	RequireLight(scene.SetObjectParent(spot,root,error),"Cannot parent spot light");
	RequireLight(scene.SetObjectParent(directional,root,error),"Cannot parent directional light");
	scene.GetEditableObject(0).transform->rotate.y = 0.4f;
	scene.GetEditableObject(0).transform->scale = {2,3,4};
	scene.GetEditableObject(2).light->outerAngle = 80;
	scene.GetEditableObject(2).light->innerAngle = 30;
	SceneLightCollection lights = SceneLighting::Collect(scene);
	RequireLight(lights.points.count == 1 && lights.spots.count == 1 && lights.directional.count == 1,"Light collection counts wrong");
	const Vector3 expected = scene.GetObjectWorldPosition(point);
	RequireLight(std::abs(lights.points.lights[0].position.y-expected.y)<0.0001f,"Point ignored parent transform");
	RequireLight(std::abs(MathUtility::Length(lights.spots.lights[0].direction)-1)<0.0001f,"Spot direction contains parent scale");
	RequireLight(lights.spots.lights[0].cosFalloffStart > lights.spots.lights[0].cosAngle,"Spot cone angles reversed");
	*scene.GetEditableObject(0).active = false;
	lights = SceneLighting::Collect(scene);
	RequireLight(lights.points.count == 0 && lights.spots.count == 0 && lights.directional.count == 0,"Inactive parent left lights enabled");
	*scene.GetEditableObject(0).active = true;
	scene.GetEditableObject(0).transform->scale = {};
	lights = SceneLighting::Collect(scene);
	RequireLight(lights.spots.count == 0 && lights.directional.count == 0,"Degenerate light direction accepted");
	scene.GetEditableObject(0).transform->scale = {1,1,1};
	EditorSession session;
	session.Initialize(scene);
	scene.GetEditableObject(2).light->innerAngle = 10;
	session.Update(scene,false);
	RequireLight(session.Undo(scene,error) && scene.GetEditableObject(2).light->innerAngle == 30,"Light angle undo failed");
	RequireLight(session.Redo(scene,error) && scene.GetEditableObject(2).light->innerAngle == 10,"Light angle redo failed");
	RequireLight(session.BeginPlay(scene,error),"Light play snapshot failed");
	scene.GetEditableObject(2).light->outerAngle = 45;
	RequireLight(session.Stop(scene,error) && scene.GetEditableObject(2).light->outerAngle == 80,"Stop lost light settings");
	const std::filesystem::path saved = directory/"lights.ponscene";
	RequireLight(SceneFile::Save(scene,saved,error),"Light scene save failed");
	SceneObjects loaded;
	RequireLight(SceneFile::Load(loaded,saved,error),"Light scene load failed");
	RequireLight(EqualLightSettings(*scene.GetEditableObject(2).light,*loaded.GetEditableObject(2).light),"Light angles lost in save");
	RequireLight(loaded.GetObjectData(3)->type == SceneObjectType::DirectionalLight,"Directional type lost in save");
	const std::filesystem::path prefab = directory/"lights.ponprefab";
	RequireLight(PrefabFile::Save(scene,root,prefab,error),"Light prefab save failed");
	RequireLight(PrefabFile::Instantiate(loaded,prefab,0,{},error) != 0,"Light prefab instantiate failed");
	lights = SceneLighting::Collect(loaded);
	RequireLight(lights.spots.count == 2 && lights.directional.count == 2,"Prefab lost lights");
	for (SceneObjectType type : {SceneObjectType::PointLight,SceneObjectType::SpotLight,SceneObjectType::DirectionalLight}) {
		SceneObjects capacity;
		for (uint32_t lightIndex = 0; lightIndex < SceneLighting::GetLimit(type); ++lightIndex) {
			RequireLight(capacity.CreateObject(type) != 0,"Light limit too small");
		}
		RequireLight(capacity.CreateObject(type) == 0,"Light limit exceeded");
		RequireLight(capacity.DuplicateObject(capacity.GetObjectData(0)->id) == 0,"Light duplication bypassed limit");
	}
	std::vector<SceneObjectData> invalid = scene.CaptureObjects();
	invalid[2].light.innerAngle = 100;
	RequireLight(!scene.ReplaceObjects(invalid,error),"Inverted spot angles accepted");
	invalid[2].light.innerAngle = std::numeric_limits<float>::quiet_NaN();
	RequireLight(!scene.ReplaceObjects(invalid,error),"Non-finite spot angle accepted");

	SceneObjects legacy;
	legacy.AddObject("plane.gltf");
	const std::filesystem::path oldPath = directory/"legacy-lights.ponscene";
	WriteLegacyScene(legacy,oldPath);
	RequireLight(SceneFile::Load(loaded,oldPath,error) && loaded.GetEditableObjectCount() == 1,"Prefab/data load injected legacy lights");
	RequireLight(SceneFile::Load(loaded,oldPath,error,true) && loaded.GetEditableObjectCount() == 4,"Legacy lights not migrated");
	lights = SceneLighting::Collect(loaded);
	RequireLight(lights.points.count == 1 && lights.spots.count == 1 && lights.directional.count == 1,"Legacy migration counts wrong");
	RequireLight(std::abs(lights.spots.lights[0].cosAngle-0.5f)<0.0001f && lights.spots.lights[0].intensity == 4,"Legacy spot appearance changed");
	RequireLight(std::abs(lights.directional.lights[0].direction.y+1)<0.0001f,"Legacy directional orientation changed");
	RequireLight(std::abs(lights.spots.lights[0].direction.x+0.70710678f)<0.0001f && std::abs(lights.spots.lights[0].direction.y+0.70710678f)<0.0001f,"Legacy spot orientation changed");
	RequireLight(SceneFile::Save(loaded,saved,error) && SceneFile::Load(loaded,saved,error,true) && loaded.GetEditableObjectCount() == 4,"Migration duplicated lights on reload");
	SceneObjects eightLights;
	for (size_t lightIndex = 0; lightIndex < 8; ++lightIndex) {
		eightLights.CreateObject(SceneObjectType::PointLight);
	}
	WriteLegacyScene(eightLights,oldPath);
	RequireLight(SceneFile::Load(loaded,oldPath,error,true),"Legacy full point scene no longer loads");
	RequireLight(SceneLighting::Collect(loaded).points.count == 9,"Legacy fixed point light was silently lost");
	SceneObjectData exhausted;
	exhausted.id = (std::numeric_limits<SceneObjectId>::max)()-1;
	std::vector<SceneObjectData> noIds = {exhausted};
	RequireLight(!SceneLighting::AddLegacyDefaults(noIds,error) && noIds.size() == 1,"Failed light migration partially changed scene");
	// 形式1の「既存IDへ適用」でも、現在の移行済みライトを二重に持ち越さない。
	SceneObjects versionOne;
	versionOne.AddObject("plane.gltf");
	const std::filesystem::path versionOnePath = directory/"legacy-light-v1.ponscene";
	std::ofstream versionOneFile(versionOnePath);
	versionOneFile << "PON_SCENE 1\n1\n1 15 5 5 5 0 0 0 12 0 0 1 1 1 1 40 1\n";
	versionOneFile.close();
	RequireLight(SceneFile::Load(versionOne,versionOnePath,error,true) && versionOne.GetEditableObjectCount() == 4,"Version1 light migration failed");
	const SceneObjectId previousNext = versionOne.GetNextObjectId();
	RequireLight(SceneFile::Load(versionOne,versionOnePath,error,true) && versionOne.GetEditableObjectCount() == 4,"Version1 repeated load duplicated lights");
	RequireLight(versionOne.GetObjectData(1)->id >= previousNext,"Migration reused retired light IDs");
}
