#include "../project/CG2/editor/EditorSession.h"
#include "SceneFile.h"
#include "IEditableScene.h"
#include "SceneObjects.h"

#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>

void RunHierarchyTests(const std::filesystem::path& directory);
void RunEditorSessionTests();
void RunSceneViewMathTests();
void RunParticlePresetTests();

namespace {
	// 検証条件を満たさなければ、失敗理由をテスト実行元へ通知する。
	void Require(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	class TestScene : public IEditableScene {
	public:
		TransformData transforms[2] = {
			{{1, 2, 3}, {0.1f, 0.2f, 0.3f}, {4, 5, 6}},
			{{2, 3, 4}, {0.4f, 0.5f, 0.6f}, {7, 8, 9}}
		};
		Vector4 colors[2] = {{0.1f, 0.2f, 0.3f, 1}, {0.4f, 0.5f, 0.6f, 1}};
		bool lighting[2] = {true, false};
		float shininess[2] = {40, 80};
		size_t GetEditableObjectCount() const override { return 2; }
		EditableObjectView GetEditableObject(size_t objectIndex) override {
			return {static_cast<SceneObjectId>(objectIndex + 1), "Test", &transforms[objectIndex], &colors[objectIndex], &lighting[objectIndex], &shininess[objectIndex]};
		}
	};

	// 不正ファイルの読込を拒否し、編集中のID・個数・値が変わらないことを確認する。
	void RequireRejectedSceneLoad(IEditableScene& scene, const std::filesystem::path& invalidScenePath,
		const std::string& fileContents, float expectedPosition) {
		std::ofstream outputFile(invalidScenePath);
		outputFile << fileContents;
		outputFile.close();

		const SceneObjectId originalObjectId = scene.GetEditableObject(0).id;
		const size_t originalObjectCount = scene.GetEditableObjectCount();
		scene.GetEditableObject(0).transform->translate.x = expectedPosition;

		std::string error;
		Require(!SceneFile::Load(scene, invalidScenePath, error), "Invalid scene file was accepted");
		Require(!error.empty(), "Missing error message");
		Require(scene.GetEditableObjectCount() == originalObjectCount, "Failed load changed object count");
		Require(scene.GetEditableObject(0).id == originalObjectId, "Failed load changed object ID");
		Require(scene.GetEditableObject(0).transform->translate.x == expectedPosition,
			"Failed load changed live data");
	}

	// 追加・削除・複製と、新旧保存形式での復元を検証する。
	void TestCollections(const std::filesystem::path& directory) {
		SceneObjects scene;
		std::string error;
		const SceneObjectId plane = scene.AddObject("plane.gltf");
		const SceneObjectId axis = scene.AddObject("axis.obj");
		Require(plane != 0 && axis > plane, "add failed");
		*scene.GetEditableObject(0).color = {0.2f, 0.3f, 0.4f, 1};
		scene.GetEditableObject(0).transform->translate = {3, 4, 5};
		*scene.GetEditableObject(0).useRootNodeMatrix = false;
		const SceneObjectId duplicate = scene.DuplicateObject(plane);
		Require(duplicate > axis, "duplicate ID reused");
		Require(scene.GetEditableObject(2).transform->translate.x == 3, "duplicate lost transform");
		scene.GetEditableObject(2).color->x = 0.9f;
		Require(scene.GetEditableObject(0).color->x == 0.2f, "duplicate shares authored material data");
		Require(scene.RemoveObject(axis) && !scene.RemoveObject(axis), "delete failed");
		Require(scene.GetEditableObject(1).id == duplicate, "delete changed another ID");
		Require(scene.AddObject("missing.obj") == 0 && scene.DuplicateObject(axis) == 0,
			"invalid operation accepted");
		*scene.GetEditableObject(1).model = "axis.obj";
		*scene.GetEditableObject(0).active = false;
		*scene.GetEditableObject(1).hasMeshRenderer = false;
		const std::filesystem::path path = directory / "collection.ponscene";
		Require(SceneFile::Save(scene, path, error), "collection save failed");

		SceneObjects restored;
		restored.AddObject("axis.obj");
		Require(SceneFile::Load(restored, path, error), "collection load failed");
		Require(restored.GetEditableObjectCount() == 2, "object count not restored");
		std::vector<SceneObjectData> snapshot = restored.CaptureObjects();
		Require(snapshot[0].id == plane && snapshot[1].id == duplicate && snapshot[0].model == "plane.gltf" &&
			snapshot[1].model == "axis.obj" && !snapshot[0].useRootNodeMatrix && snapshot[1].color.x == 0.9f,
			"object metadata not restored");
		Require(!snapshot[0].active && !snapshot[1].hasMeshRenderer, "component state not restored");
		const SceneObjectId freshId = restored.AddObject("axis.obj");
		Require(freshId > duplicate, "loaded ID reused");
		Require(SceneFile::Load(restored, path, error), "collection reload failed");
		// 異なる種類が混在しても、設定・型・複製・履歴を失わず保存できる。
		SceneObjects mixed;
		Require(mixed.CreateObject(SceneObjectType::Empty) != 0, "cannot create empty object");
		Require(mixed.CreateObject(SceneObjectType::PointLight) != 0, "cannot create light");
		const SceneObjectId emitterId = mixed.CreateObject(SceneObjectType::Particle);
		Require(emitterId != 0, "cannot create emitter");
		*mixed.GetEditableObject(1).lightIntensity = 3.5f;
		*mixed.GetEditableObject(1).lightRadius = 22;
		mixed.GetEditableObject(2).particleSettings->spawnCount = 17;
		mixed.GetEditableObject(2).particleSettings->texture = "resources/white1x1.png";
		mixed.GetEditableObject(2).transform->translate = {4,2,1};
		Require(mixed.DuplicateObject(emitterId) != 0, "cannot duplicate emitter");
		const std::filesystem::path mixedPath = directory / "mixed.ponscene";
		Require(SceneFile::Save(mixed, mixedPath, error), "cannot save mixed scene");
		SceneObjects loadedMixed;
		Require(SceneFile::Load(loadedMixed, mixedPath, error), "cannot load mixed scene");
		EditorSession roundtripSession;
		roundtripSession.Initialize(mixed);
		Require(!roundtripSession.IsDirty(loadedMixed), "mixed scene lost component values");
		Require(!*loadedMixed.GetEditableObject(0).hasMeshRenderer && loadedMixed.GetEditableObject(1).type == SceneObjectType::PointLight,
			"empty or light type changed");
		EditorSession mixedSession;
		mixedSession.Initialize(mixed);
		mixed.GetEditableObject(2).particleSettings->spawnCount = 29;
		mixedSession.Update(mixed, false);
		Require(mixedSession.Undo(mixed, error) && mixed.GetEditableObject(2).particleSettings->spawnCount == 17, "emitter undo failed");
		Require(mixedSession.Redo(mixed, error) && mixed.GetEditableObject(2).particleSettings->spawnCount == 29, "emitter redo failed");
		Require(mixedSession.BeginPlay(mixed, error), "cannot play mixed scene");
		*mixed.GetEditableObject(1).lightIntensity = 99;
		Require(mixedSession.Stop(mixed, error) && *mixed.GetEditableObject(1).lightIntensity == 3.5f, "light stop restore failed");
		for (size_t lightIndex = 1; lightIndex < 8; ++lightIndex) {
			Require(mixed.CreateObject(SceneObjectType::PointLight) != 0, "light capacity too small");
		}
		Require(mixed.CreateObject(SceneObjectType::PointLight) == 0, "light capacity exceeded");
		std::vector<SceneObjectData> invalidObjects = mixed.CaptureObjects();
		invalidObjects[1].lightRadius = 0;
		Require(!mixed.ReplaceObjects(invalidObjects, error), "invalid light accepted");

		Require(restored.AddObject("axis.obj") > freshId, "reload moved ID allocator backwards");

		const std::vector<SceneObjectData> before = restored.CaptureObjects();
		std::vector<SceneObjectData> invalid = before;
		invalid[1].id = invalid[0].id;
		Require(!restored.ReplaceObjects(invalid, error) && restored.GetEditableObjectCount() == before.size(),
			"duplicate replacement accepted");
		invalid = before;
		invalid[1].model = "../missing.obj";
		Require(!restored.ReplaceObjects(invalid, error), "unknown asset accepted");
		invalid = before;
		invalid[1].transform.translate.x = std::numeric_limits<float>::infinity();
		Require(!restored.ReplaceObjects(invalid, error), "infinite data accepted");

		std::ifstream inputFile(path);
		std::string header, count, first, second;
		std::getline(inputFile, header); std::getline(inputFile, count);
		std::getline(inputFile, first); std::getline(inputFile, second);
		inputFile.close();
		const std::filesystem::path broken = directory / "collection-broken.ponscene";
		RequireRejectedSceneLoad(restored, broken, "PON_SCENE 4\n2\n" + first + "\n" + first, 777);
		RequireRejectedSceneLoad(restored, broken, "PON_SCENE 4\n2\n" + first + "\n3 15", 777);
		RequireRejectedSceneLoad(restored, broken, "PON_SCENE 4\n2\n" + first + "\n" + second + "\nextra", 777);
		std::string unsupported = second;
		unsupported.replace(unsupported.rfind("axis.obj"), 8, "missing.obj");
		RequireRejectedSceneLoad(restored, broken, "PON_SCENE 4\n2\n" + first + "\n" + unsupported, 777);
		const std::string invalidType = "4 15 1 1 1 0 0 0 0 0 0 1 1 1 1 40 1 \"Invalid\" \"plane.gltf\" 1 1 0 99 1 10 1";
		RequireRejectedSceneLoad(restored, broken, "PON_SCENE 4\n2\n" + first + "\n" + invalidType, 777);

		const std::filesystem::path versionTwoPath = directory / "version-two.ponscene";
		// 旧形式は現在の出力から切り出さず、当時のレコードを固定fixtureとして用意する。
		const std::string firstWithoutFlags = "1 15 1 1 1 0 0 0 3 4 5 0.2 0.3 0.4 1 40 1 \"Plane legacy\" \"plane.gltf\" 0";
		const std::string secondWithoutFlags = "3 15 1 1 1 0 0 0 3 4 5 0.9 0.3 0.4 1 40 1 \"Axis legacy\" \"axis.obj\" 1";
		std::ofstream versionTwoFile(versionTwoPath);
		versionTwoFile << "PON_SCENE 2\n2\n" << firstWithoutFlags << '\n' << secondWithoutFlags << '\n';
		versionTwoFile.close();
		SceneObjects versionTwoScene;
		Require(SceneFile::Load(versionTwoScene, versionTwoPath, error), "version 2 migration failed");
		Require(*versionTwoScene.GetEditableObject(0).active && *versionTwoScene.GetEditableObject(1).hasMeshRenderer,
			"version 2 component defaults incorrect");

		const std::filesystem::path versionThreePath = directory / "version-three.ponscene";
		std::ofstream versionThreeFile(versionThreePath);
		versionThreeFile << "PON_SCENE 3\n2\n" << firstWithoutFlags << " 0 1\n" << secondWithoutFlags << " 1 0\n";
		versionThreeFile.close();
		SceneObjects versionThreeScene;
		Require(SceneFile::Load(versionThreeScene, versionThreePath, error), "version 3 migration failed");
		Require(!*versionThreeScene.GetEditableObject(0).active && !*versionThreeScene.GetEditableObject(1).hasMeshRenderer,
			"version 3 flags lost");
		RequireRejectedSceneLoad(restored, broken, "PON_SCENE 3\n2\n" + firstWithoutFlags + " 1 1\n" + secondWithoutFlags + " 1 2", 777);

		// 空のシーンは意図した保存状態として復元し、既定モデルを勝手に再生成しない。
		Require(scene.ReplaceObjects({}, error), "clear failed");
		Require(SceneFile::Save(scene, path, error) && SceneFile::Load(restored, path, error) &&
			restored.GetEditableObjectCount() == 0, "empty scene roundtrip failed");
		Require(restored.AddObject("axis.obj") != 0, "cannot add after empty scene load");

		// 旧形式のモデル設定を読み込み、保存対象外だった二体目の既定モデルも残す。
		SceneObjects legacy;
		legacy.AddObject("plane.gltf"); legacy.AddObject("axis.obj");
		const std::filesystem::path legacyPath = directory / "legacy-single.ponscene";
		std::ofstream legacyOutputFile(legacyPath);
		legacyOutputFile << "PON_SCENE 1\n1\n1 15 5 5 5 0 0 0 12 0 0 1 1 1 1 40 1\n";
		legacyOutputFile.close();
		Require(SceneFile::Load(legacy, legacyPath, error), "version 1 migration failed");
		Require(legacy.GetEditableObjectCount() == 2 && legacy.GetEditableObject(0).transform->translate.x == 12 &&
			*legacy.GetEditableObject(1).model == "axis.obj", "version 1 migration lost data");
	}
}

// テスト用ディレクトリだけを使用し、ユーザーが保存したシーンを変更せずに検証する。
int main() {
	try {
		const std::filesystem::path directory = "generated/scene-file-tests/data";
		const std::filesystem::path path = directory / "roundtrip.ponscene";
		TestScene scene;
		std::string error;
		Require(SceneFile::Save(scene, path, error), "save failed");
		scene.transforms[0].translate.x = 123;
		scene.transforms[1].rotate.y = 456;
		scene.colors[0].x = 0;
		scene.lighting[0] = false;
		scene.shininess[1] = 1;
		Require(SceneFile::Load(scene, path, error), "load failed");
		Require(scene.transforms[0].translate.x == 4 && scene.transforms[1].rotate.y == 0.5f,
			"transform roundtrip failed");
		Require(scene.colors[0].x == 0.1f && scene.lighting[0] && scene.shininess[1] == 80,
			"appearance roundtrip failed");

		// 後半のレコードだけを壊し、前半まで含めて現在のシーンが変更されないことを検証する。
		std::ifstream inputFile(path);
		std::string header, count, first, second;
		std::getline(inputFile, header); std::getline(inputFile, count);
		std::getline(inputFile, first); std::getline(inputFile, second);
		inputFile.close();
		const std::filesystem::path broken = directory / "broken.ponscene";
		RequireRejectedSceneLoad(scene, broken, "PON_SCENE 99\n2\n" + first + "\n" + second, 999);
		RequireRejectedSceneLoad(scene, broken, "PON_SCENE 1\n2\n" + first + "\n2 15 1", 999);
		RequireRejectedSceneLoad(scene, broken, "PON_SCENE 1\n2\n" + first + "\n" + first, 999);
		RequireRejectedSceneLoad(scene, broken, "PON_SCENE 1\n2\n" + first + "\n" + second + "\ntrailing", 999);
		RequireRejectedSceneLoad(scene, broken, "PON_SCENE 1\n2\n" + first + "\n3" + second.substr(1), 999);
		Require(!SceneFile::Load(scene, directory / "missing.ponscene", error), "missing file accepted");

		// 保存を拒否した場合も、直前の正常な保存ファイルが残ることを確認する。
		scene.shininess[0] = -1;
		Require(!SceneFile::Save(scene, path, error), "invalid save accepted");
		Require(SceneFile::Load(scene, path, error), "rejected save damaged existing file");
		Require(scene.shininess[0] == 40, "saved file changed");
		// 同じ保存先へ再度書き込み、既存ファイルの置換も確認する。
		scene.transforms[0].translate.x = 42;
		Require(SceneFile::Save(scene, path, error), "replacement save failed");
		scene.transforms[0].translate.x = 0;
		Require(SceneFile::Load(scene, path, error) && scene.transforms[0].translate.x == 42,
			"replacement roundtrip failed");
		TestCollections(directory);
		RunHierarchyTests(directory);
		RunEditorSessionTests();
		RunSceneViewMathTests();
		RunParticlePresetTests();
		std::cout << "Scene, history, viewport and particle preset tests passed\n";
		return 0;
	} catch (const std::exception& exception) {
		std::cerr << exception.what() << '\n';
		return 1;
	}
}
