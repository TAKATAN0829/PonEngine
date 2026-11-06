#include "serialization/ParticlePresetFile.h"
#include "serialization/SceneFile.h"
#include "objects/WorldObjects.h"
#include "prefab/PrefabOperations.h"
#include "../project/CG2/editor/state/EditorSession.h"
#include <sstream>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

namespace {
	// 失敗した条件を実行元へ返し、検証漏れを成功扱いにしない。
	void RequireParticleCondition(bool isConditionSatisfied, const char* message) {
		if (!isConditionSatisfied) {
			throw std::runtime_error(message);
		}
	}
}

// 正常な往復保存と、不正ファイルで編集中の設定が変わらないことを確認する。
void RunParticlePresetTests() {
	const std::filesystem::path directory = "generated/scene-file-tests/particles";
	const std::filesystem::path presetPath = directory / "test.ponparticle";
	ParticleSettings settings;
	settings = MakeParticlePreset(ParticlePresetKind::Smoke);
	settings.emitterPosition = {1,2,3};
	settings.spread = {2,0.5f,3};
	settings.velocityMinimum = {-3,-2,-1};
	settings.velocityMaximum = {3,4,5};
	settings.particleScale = {0.3f,0.4f,0.5f};
	settings.color = {0.2f,0.4f,0.8f,0.6f};
	settings.acceleration = {0,-9.8f,0};
	settings.isAccelerationEnabled = true;
	settings.isRandomColorEnabled = true;
	settings.isBillboardEnabled = false;
	settings.spawnCount = 80;
	settings.interval = 0.03f;
	settings.lifetimeMinimum = 0.5f;
	settings.lifetimeMaximum = 5;
	settings.texture = "resources/uvChecker.png";
	settings.blendMode = kBlendModeScreen;
	const ParticleSettings original = settings;
	std::string error;
	RequireParticleCondition(ParticlePresetFile::Save(settings, presetPath, error), "Particle save failed");
	settings = {};
	RequireParticleCondition(ParticlePresetFile::Load(settings, presetPath, error) && AreParticleSettingsEqual(settings, original),
		"Particle settings did not roundtrip");
	// 不正な値での保存を拒否し、先に保存したプリセットを読み直せることを確認する。
	settings.spawnCount = 0;
	RequireParticleCondition(!ParticlePresetFile::Save(settings, presetPath, error), "Invalid emission count was saved");
	RequireParticleCondition(ParticlePresetFile::Load(settings, presetPath, error) && AreParticleSettingsEqual(settings, original),
		"Rejected save damaged the existing preset");
	settings.velocityMinimum.x = 100;
	RequireParticleCondition(!AreParticleSettingsValid(settings), "Inverted velocity range was accepted");
	settings = original;
	settings.lifetimeMinimum = 8;
	RequireParticleCondition(!AreParticleSettingsValid(settings), "Inverted lifetime range was accepted");
	settings = original;
	settings.interval = std::numeric_limits<float>::quiet_NaN();
	RequireParticleCondition(!AreParticleSettingsValid(settings), "Non-finite particle setting was accepted");

	std::ifstream source(presetPath);
	const std::string contents((std::istreambuf_iterator<char>(source)), std::istreambuf_iterator<char>());
	source.close();
	const std::filesystem::path brokenPath = directory / "broken.ponparticle";
	std::ofstream broken(brokenPath);
	broken << "PON_PARTICLE 99\n";
	broken.close();
	settings = original;
	RequireParticleCondition(!ParticlePresetFile::Load(settings, brokenPath, error) && AreParticleSettingsEqual(settings, original),
		"Rejected load changed particle settings");
	broken.open(brokenPath);
	broken << contents << "extra";
	broken.close();
	RequireParticleCondition(!ParticlePresetFile::Load(settings, brokenPath, error), "Trailing preset data was accepted");
	std::string unknownTexture = contents;
	unknownTexture.replace(unknownTexture.find("resources/uvChecker.png"), 23, "../unknown.png");
	broken.open(brokenPath);
	broken << unknownTexture;
	broken.close();
	RequireParticleCondition(!ParticlePresetFile::Load(settings, brokenPath, error) && AreParticleSettingsEqual(settings, original),
		"Unknown particle texture was accepted");
	RequireParticleCondition(!ParticlePresetFile::Load(settings, directory / "missing", error), "Missing preset was accepted");

	// 旧形式1は新規項目を補完し、従来のサイズ固定・Alpha減衰を維持する。
	std::string legacy = contents;
	const size_t extensionStart = legacy.rfind('\n',legacy.size()-2)+1;
	legacy.erase(extensionStart);
	legacy.replace(0,std::string("PON_PARTICLE 2").size(),"PON_PARTICLE 1");
	broken.open(brokenPath);
	broken << legacy;
	broken.close();
	RequireParticleCondition(ParticlePresetFile::Load(settings,brokenPath,error),"Legacy particle preset no longer loads");
	RequireParticleCondition(!settings.areSoftParticlesEnabled && !settings.isLifetimeAppearanceEnabled,"Legacy particle appearance changed");
	const ParticleAppearance oldAppearance = EvaluateParticleAppearance(settings,{1,0.5f,0.2f,0.8f},0.25f);
	RequireParticleCondition(std::abs(oldAppearance.color.w-0.6f)<0.0001f && oldAppearance.size == 1,"Legacy lifetime fade changed");

	for (ParticlePresetKind kind : {ParticlePresetKind::Fire,ParticlePresetKind::Smoke,ParticlePresetKind::Sparks}) {
		const ParticleSettings preset = MakeParticlePreset(kind);
		RequireParticleCondition(AreParticleSettingsValid(preset),"Built-in particle preset invalid");
		const ParticleAppearance birth = EvaluateParticleAppearance(preset,preset.color,0);
		const ParticleAppearance middle = EvaluateParticleAppearance(preset,preset.color,0.5f);
		const ParticleAppearance death = EvaluateParticleAppearance(preset,preset.color,1);
		RequireParticleCondition(std::abs(middle.size-(preset.startSize+preset.endSize)*0.5f)<0.0001f,"Particle size interpolation failed");
		RequireParticleCondition(std::abs(middle.color.x-(preset.color.x+preset.endColor.x)*0.5f)<0.0001f,"Particle color interpolation failed");
		RequireParticleCondition(death.color.w == 0 && death.size == preset.endSize,"Particle death appearance incorrect");
		if (preset.fadeIn > 0) {
			RequireParticleCondition(birth.color.w == 0,"Particle fade-in pops at birth");
		}
	}
	settings = MakeParticlePreset(ParticlePresetKind::Smoke);
	settings.softDistance = std::numeric_limits<float>::infinity();
	RequireParticleCondition(!AreParticleSettingsValid(settings),"Non-finite soft distance accepted");
	settings = original;
	settings.endColor.x = std::numeric_limits<float>::quiet_NaN();
	RequireParticleCondition(!AreParticleSettingsValid(settings),"Non-finite end color accepted");

	// Scene・Prefab・Undoでも新しい項目を失わず、旧Scene11のParticleも読める。
	WorldObjects scene;
	const WorldObjectId emitter = scene.CreateObject(WorldObjectType::Particle);
	*scene.GetEditableObject(0).particleSettings = original;
	EditorSession session;
	session.Initialize(scene);
	scene.GetEditableObject(0).particleSettings->endSize = 0.12f;
	session.Update(scene,false);
	RequireParticleCondition(session.Undo(scene,error) && AreParticleSettingsEqual(*scene.GetEditableObject(0).particleSettings,original),"Particle lifetime undo lost fields");
	RequireParticleCondition(session.Redo(scene,error),"Particle lifetime redo failed");
	const std::filesystem::path scenePath = directory/"lifetime.ponscene";
	RequireParticleCondition(SceneFile::Save(scene,scenePath,error),"Particle lifetime scene save failed");
	WorldObjects loaded;
	RequireParticleCondition(SceneFile::Load(loaded,scenePath,error) && AreParticleSettingsEqual(*scene.GetEditableObject(0).particleSettings,*loaded.GetEditableObject(0).particleSettings),"Particle lifetime scene roundtrip failed");
	const std::filesystem::path prefabPath = directory/"lifetime.ponprefab";
	RequireParticleCondition(PrefabOperations::Save(scene,emitter,prefabPath,error),"Particle lifetime prefab save failed");
	RequireParticleCondition(PrefabOperations::Instantiate(loaded,prefabPath,0,{},error) != 0,"Particle lifetime prefab instantiate failed");
	RequireParticleCondition(AreParticleSettingsEqual(*scene.GetEditableObject(0).particleSettings,*loaded.GetEditableObject(1).particleSettings),"Prefab lost lifetime appearance");
	std::ifstream savedScene(scenePath);
	std::string legacyScene((std::istreambuf_iterator<char>(savedScene)),std::istreambuf_iterator<char>());
	std::ostringstream serializedParticle;
	serializedParticle << std::setprecision(std::numeric_limits<float>::max_digits10);
	ParticlePresetFile::WriteSettings(serializedParticle,*scene.GetEditableObject(0).particleSettings);
	const std::string particleText = serializedParticle.str();
	const std::string extension = particleText.substr(particleText.rfind('\n',particleText.size()-2)+1);
	legacyScene.erase(legacyScene.find(extension),extension.size());
	const std::string lightExtension = " 60 0\n";
	legacyScene.erase(legacyScene.rfind(lightExtension),lightExtension.size()-1);
	legacyScene.replace(0,std::string("PON_SCENE 13").size(),"PON_SCENE 11");
	std::ofstream oldScene(directory/"legacy11.ponscene");
	oldScene << legacyScene;
	oldScene.close();
	RequireParticleCondition(SceneFile::Load(loaded,directory/"legacy11.ponscene",error),"Scene11 particle compatibility broken");
	RequireParticleCondition(!loaded.GetEditableObject(0).particleSettings->areSoftParticlesEnabled && !loaded.GetEditableObject(0).particleSettings->isLifetimeAppearanceEnabled,"Scene11 defaults changed");
}
