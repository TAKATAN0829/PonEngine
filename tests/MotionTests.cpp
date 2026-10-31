#include "SceneMotion.h"
#include "SceneObjects.h"
#include "SceneFile.h"
#include "PrefabFile.h"
#include "../project/CG2/editor/EditorSession.h"
#include <cmath>
#include <fstream>
#include <limits>
#include <numbers>
#include <stdexcept>

namespace {
	void RequireMotion(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
	bool NearMotion(float first, float second) {
		return std::abs(first - second) < 0.0001f;
	}

	void VerifyMotionRuntime() {
		SceneObjects scene;
		const SceneObjectId parent = scene.CreateObject(SceneObjectType::Empty);
		const SceneObjectId child = scene.AddObject("plane.gltf");
		std::string error;
		RequireMotion(scene.SetObjectParent(child, parent, error), "Motion parenting failed");
		scene.GetEditableObject(0).transform->translate = {10,0,0};
		scene.GetEditableObject(1).transform->translate = {1,2,3};
		SceneMotionSettings& settings = *scene.GetEditableObject(1).motion;
		settings.hasRotation = true;
		settings.rotationDegreesPerSecond = {0,90,0};
		settings.hasMovement = true;
		settings.movementOffset = {4,0,0};
		settings.movementPeriod = 2;
		SceneMotion runtime;
		runtime.Initialize();
		runtime.Update(scene, 0.5f);
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 3), "Quarter-period movement incorrect");
		RequireMotion(NearMotion(scene.GetObjectWorldPosition(child).x, 13), "Movement ignored parent space");
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.rotate.y, std::numbers::pi_v<float> / 4), "Rotation degrees/seconds incorrect");
		runtime.Update(scene, 0.5f);
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 5), "Movement endpoint incorrect");
		runtime.Update(scene, 1);
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 1), "Movement did not return to start");
		// 無効な親の子は止まり、有効化後は同じ位相から再開する。
		*scene.GetEditableObject(0).active = false;
		runtime.Update(scene, 0.5f);
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 1), "Inactive hierarchy kept moving");
		*scene.GetEditableObject(0).active = true;
		settings.rotationEnabled = false;
		settings.movementEnabled = false;
		const float stoppedAngle = scene.GetObjectDataById(child)->transform.rotate.y;
		runtime.Update(scene, 0.5f);
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 1) && scene.GetObjectDataById(child)->transform.rotate.y == stoppedAngle, "Disabled component kept updating");
		settings.movementEnabled = true;
		runtime.Update(scene, 0.5f);
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 3), "Disabled movement advanced its clock");
		runtime.Update(scene, -1);
		runtime.Update(scene, (std::numeric_limits<float>::quiet_NaN)());
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 3), "Invalid delta changed motion");
		settings.hasMovement = false;
		runtime.Update(scene, 0.5f);
		scene.GetEditableObject(1).transform->translate.x = 20;
		settings.hasMovement = true;
		runtime.Update(scene, 0.5f);
		RequireMotion(NearMotion(scene.GetObjectDataById(child)->transform.translate.x, 22), "Readded component reused stale origin");
		RequireMotion(scene.RemoveObject(child), "Motion object removal failed");
		runtime.Update(scene, 0.5f);
		runtime.Finalize();
	}

	void VerifyMotionHistoryAndPlay(const std::filesystem::path& directory) {
		SceneObjects scene;
		const SceneObjectId identifier = scene.AddObject("plane.gltf");
		EditorSession session;
		session.Initialize(scene);
		scene.GetEditableObject(0).motion->hasRotation = true;
		scene.GetEditableObject(0).motion->hasMovement = true;
		scene.GetEditableObject(0).motion->movementOffset = {2,4,6};
		const SceneMotionSettings expected = *scene.GetEditableObject(0).motion;
		session.Update(scene, false);
		std::string error;
		RequireMotion(session.Undo(scene, error) && !scene.GetObjectData(0)->motion.hasRotation, "Motion add missing from undo");
		RequireMotion(session.Redo(scene, error) && EqualMotionSettings(scene.GetObjectData(0)->motion, expected), "Motion settings missing from redo");
		RequireMotion(SceneFile::Save(scene, directory / "motion.ponscene", error), "Motion scene save failed");
		SceneObjects restored;
		RequireMotion(SceneFile::Load(restored, directory / "motion.ponscene", error), "Motion scene load failed");
		RequireMotion(EqualMotionSettings(restored.GetObjectData(0)->motion, expected), "Motion roundtrip failed");
		RequireMotion(PrefabFile::Save(scene, identifier, directory / "motion.ponprefab", error), "Motion prefab save failed");
		RequireMotion(PrefabFile::Instantiate(restored, directory / "motion.ponprefab", 0, {}, error) != 0, "Motion prefab placement failed");
		RequireMotion(EqualMotionSettings(restored.GetObjectData(1)->motion, expected), "Prefab lost motion settings");
		RequireMotion(session.BeginPlay(scene, error), "Motion Play failed");
		SceneMotion runtime;
		runtime.Initialize();
		runtime.Update(scene, 0.5f);
		RequireMotion(scene.GetObjectData(0)->transform.translate.y > 0, "Motion did not run in Play");
		session.TogglePause();
		RequireMotion(!session.ShouldSimulate(), "Pause did not stop simulation");
		session.RequestStep();
		RequireMotion(session.ShouldSimulate() && !session.ShouldSimulate(), "Step did not advance exactly once");
		runtime.Update(scene, 1.0f / 60);
		RequireMotion(session.Stop(scene, error), "Motion Stop failed");
		runtime.Initialize();
		RequireMotion(scene.GetObjectData(0)->transform.translate.y == 0 && scene.GetObjectData(0)->transform.rotate.y == 0, "Stop did not restore editing transform");
		RequireMotion(EqualMotionSettings(scene.GetObjectData(0)->motion, expected), "Stop discarded component configuration");
		RequireMotion(session.BeginPlay(scene, error), "Second Motion Play failed");
		runtime.Initialize();
		runtime.Update(scene, 0.5f);
		RequireMotion(NearMotion(scene.GetObjectData(0)->transform.translate.y, 2), "Second Play reused old motion clock");
		RequireMotion(session.Stop(scene, error), "Second Motion Stop failed");
		std::vector<SceneObjectData> invalid = scene.CaptureObjects();
		invalid[0].motion.movementPeriod = 0;
		RequireMotion(!scene.ReplaceObjects(invalid, error), "Zero movement period accepted");
		invalid[0].motion = expected;
		invalid[0].motion.rotationDegreesPerSecond.x = (std::numeric_limits<float>::infinity)();
		RequireMotion(!scene.ReplaceObjects(invalid, error), "Infinite motion speed accepted");
		// 保存形式8は動作Componentなしとして読み込める。
		std::ifstream current(directory / "motion.ponscene");
		std::ofstream legacy(directory / "motion-v8.ponscene");
		std::string record;
		std::getline(current, record);
		legacy << "PON_SCENE 8\n";
		std::getline(current, record);
		legacy << record << '\n';
		std::getline(current, record);
		const size_t lastQuote = record.rfind('"');
		RequireMotion(lastQuote != std::string::npos, "Legacy conversion missing prefab source");
		legacy << record.substr(0, lastQuote + 1) << '\n';
		legacy.close();
		RequireMotion(SceneFile::Load(restored, directory / "motion-v8.ponscene", error) && !restored.GetObjectData(0)->motion.hasRotation, "Format 8 compatibility failed");
		std::ofstream malformed(directory / "motion-invalid.ponscene");
		malformed << "PON_SCENE 9\n1\n" << record.substr(0, lastQuote + 1) << " 2 1 0 45 0 1 1 0 2 0 2\n";
		malformed.close();
		RequireMotion(!SceneFile::Load(restored, directory / "motion-invalid.ponscene", error), "Invalid motion flag accepted");
		RequireMotion(restored.GetEditableObjectCount() == 1 && !restored.GetObjectData(0)->motion.hasRotation, "Failed motion load partially changed scene");
	}
}

void RunMotionTests(const std::filesystem::path& directory) {
	VerifyMotionRuntime();
	VerifyMotionHistoryAndPlay(directory);
}
