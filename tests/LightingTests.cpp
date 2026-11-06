#include "serialization/LegacyLightMigration.h"
#include "systems/WorldLighting.h"
#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include "prefab/PrefabOperations.h"
#include "../project/CG2/editor/state/EditorSession.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>

namespace {
	void RequireLight(bool isConditionSatisfied, const char* message) {
		if (!isConditionSatisfied) {
			throw std::runtime_error(message);
		}
	}
	// 新しいモデルデータから旧Scene12のレコードを作り、本番の移行経路へ渡す。
	void WriteLegacyScene(WorldObjects& source, const std::filesystem::path& path) {
		std::string error;
		RequireLight(SceneFile::Save(source,path,error),"Legacy lighting fixture save failed");
		std::ifstream input(path);
		std::ostringstream converted;
		std::string line;
		std::getline(input,line);
		converted << "PON_SCENE 12\n";
		while (std::getline(input,line)) {
			const std::string suffix = " 60 0";
			// 現行データの末尾からライト設定を外し、旧形式の読込を試せるようにする。
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
	WorldObjects scene;
	const WorldObjectId root = scene.CreateObject(WorldObjectType::Empty);
	const WorldObjectId point = scene.CreateObject(WorldObjectType::PointLight);
	const WorldObjectId spot = scene.CreateObject(WorldObjectType::SpotLight);
	const WorldObjectId directional = scene.CreateObject(WorldObjectType::DirectionalLight);
	RequireLight(root != 0 && point != 0 && spot != 0 && directional != 0,"Cannot create all light types");
	std::string error;
	RequireLight(scene.SetObjectParent(point,root,error),"Cannot parent point light");
	RequireLight(scene.SetObjectParent(spot,root,error),"Cannot parent spot light");
	RequireLight(scene.SetObjectParent(directional,root,error),"Cannot parent directional light");
	scene.GetEditableObject(0).transform->rotate.y = 0.4f;
	scene.GetEditableObject(0).transform->scale = {2,3,4};
	scene.GetEditableObject(2).light->outerAngle = 80;
	scene.GetEditableObject(2).light->innerAngle = 30;
	WorldLightCollection lights = WorldLighting::Collect(scene);
	RequireLight(lights.points.count == 1 && lights.spots.count == 1 && lights.directional.count == 1,"Light collection counts wrong");
	const Vector3 expected = scene.GetObjectWorldPosition(point);
	RequireLight(std::abs(lights.points.lights[0].position.y-expected.y)<0.0001f,"Point ignored parent transform");
	RequireLight(std::abs(MathUtility::Length(lights.spots.lights[0].direction)-1)<0.0001f,"Spot direction contains parent scale");
	RequireLight(lights.spots.lights[0].cosFalloffStart > lights.spots.lights[0].cosAngle,"Spot cone angles reversed");
	*scene.GetEditableObject(0).isActive = false;
	lights = WorldLighting::Collect(scene);
	RequireLight(lights.points.count == 0 && lights.spots.count == 0 && lights.directional.count == 0,"Inactive parent left lights enabled");
	*scene.GetEditableObject(0).isActive = true;
	scene.GetEditableObject(0).transform->scale = {};
	lights = WorldLighting::Collect(scene);
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
	WorldObjects loaded;
	RequireLight(SceneFile::Load(loaded,saved,error),"Light scene load failed");
	RequireLight(AreLightSettingsEqual(*scene.GetEditableObject(2).light,*loaded.GetEditableObject(2).light),"Light angles lost in save");
	RequireLight(loaded.GetObjectData(3)->type == WorldObjectType::DirectionalLight,"Directional type lost in save");
	const std::filesystem::path prefab = directory/"lights.ponprefab";
	RequireLight(PrefabOperations::Save(scene,root,prefab,error),"Light prefab save failed");
	RequireLight(PrefabOperations::Instantiate(loaded,prefab,0,{},error) != 0,"Light prefab instantiate failed");
	lights = WorldLighting::Collect(loaded);
	RequireLight(lights.spots.count == 2 && lights.directional.count == 2,"Prefab lost lights");
	for (WorldObjectType type : {WorldObjectType::PointLight,WorldObjectType::SpotLight,WorldObjectType::DirectionalLight}) {
		WorldObjects capacity;
		for (uint32_t lightIndex = 0; lightIndex < LightTypes::GetLimit(type); ++lightIndex) {
			RequireLight(capacity.CreateObject(type) != 0,"Light limit too small");
		}
		RequireLight(capacity.CreateObject(type) == 0,"Light limit exceeded");
		RequireLight(capacity.DuplicateObject(capacity.GetObjectData(0)->id) == 0,"Light duplication bypassed limit");
	}
	std::vector<WorldObjectData> invalid = scene.CaptureObjects();
	invalid[2].light.innerAngle = 100;
	RequireLight(!scene.ReplaceObjects(invalid,error),"Inverted spot angles accepted");
	invalid[2].light.innerAngle = std::numeric_limits<float>::quiet_NaN();
	RequireLight(!scene.ReplaceObjects(invalid,error),"Non-finite spot angle accepted");

	WorldObjects legacy;
	legacy.AddObject("plane.gltf");
	const std::filesystem::path oldPath = directory/"legacy-lights.ponscene";
	WriteLegacyScene(legacy,oldPath);
	RequireLight(SceneFile::Load(loaded,oldPath,error) && loaded.GetEditableObjectCount() == 1,"Prefab/data load injected legacy lights");
	RequireLight(SceneFile::Load(loaded,oldPath,error,true) && loaded.GetEditableObjectCount() == 4,"Legacy lights not migrated");
	lights = WorldLighting::Collect(loaded);
	RequireLight(lights.points.count == 1 && lights.spots.count == 1 && lights.directional.count == 1,"Legacy migration counts wrong");
	RequireLight(std::abs(lights.spots.lights[0].cosAngle-0.5f)<0.0001f && lights.spots.lights[0].intensity == 4,"Legacy spot appearance changed");
	RequireLight(std::abs(lights.directional.lights[0].direction.y+1)<0.0001f,"Legacy directional orientation changed");
	RequireLight(std::abs(lights.spots.lights[0].direction.x+0.70710678f)<0.0001f && std::abs(lights.spots.lights[0].direction.y+0.70710678f)<0.0001f,"Legacy spot orientation changed");
	RequireLight(SceneFile::Save(loaded,saved,error) && SceneFile::Load(loaded,saved,error,true) && loaded.GetEditableObjectCount() == 4,"Migration duplicated lights on reload");
	WorldObjects eightLights;
	for (size_t lightIndex = 0; lightIndex < 8; ++lightIndex) {
		eightLights.CreateObject(WorldObjectType::PointLight);
	}
	WriteLegacyScene(eightLights,oldPath);
	RequireLight(SceneFile::Load(loaded,oldPath,error,true),"Legacy full point scene no longer loads");
	RequireLight(WorldLighting::Collect(loaded).points.count == 9,"Legacy fixed point light was silently lost");
	WorldObjectData exhausted;
	exhausted.id = (std::numeric_limits<WorldObjectId>::max)()-1;
	std::vector<WorldObjectData> noIds = {exhausted};
	RequireLight(!LegacyLightMigration::AddLegacyDefaults(noIds,error) && noIds.size() == 1,"Failed light migration partially changed scene");
	// 追加後のライト上限検証で拒否されても、元の配列と設定を維持する。
	std::vector<WorldObjectData> fullPointLights = eightLights.CaptureObjects();
	WorldObjectData lastPointLight = fullPointLights.back();
	lastPointLight.id = eightLights.GetNextObjectId();
	lastPointLight.color = {0.25f,0.5f,0.75f,1};
	fullPointLights.push_back(lastPointLight);
	RequireLight(!LegacyLightMigration::AddLegacyDefaults(fullPointLights,error),"Migration exceeded point light capacity");
	RequireLight(fullPointLights.size() == kMaxPointLights,"Rejected migration appended lights");
	RequireLight(fullPointLights.back().id == lastPointLight.id && fullPointLights.back().color.x == 0.25f,"Rejected migration changed existing lights");
	// 形式1の「既存IDへ適用」でも、現在の移行済みライトを二重に持ち越さない。
	WorldObjects versionOne;
	versionOne.AddObject("plane.gltf");
	const std::filesystem::path versionOnePath = directory/"legacy-light-v1.ponscene";
	std::ofstream versionOneFile(versionOnePath);
	versionOneFile << "PON_SCENE 1\n1\n1 15 5 5 5 0 0 0 12 0 0 1 1 1 1 40 1\n";
	versionOneFile.close();
	RequireLight(SceneFile::Load(versionOne,versionOnePath,error,true) && versionOne.GetEditableObjectCount() == 4,"Version1 light migration failed");
	const WorldObjectId previousNext = versionOne.GetNextObjectId();
	RequireLight(SceneFile::Load(versionOne,versionOnePath,error,true) && versionOne.GetEditableObjectCount() == 4,"Version1 repeated load duplicated lights");
	RequireLight(versionOne.GetObjectData(1)->id >= previousNext,"Migration reused retired light IDs");
}
