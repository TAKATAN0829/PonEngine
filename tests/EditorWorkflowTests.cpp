#include "../project/CG2/editor/EditorSelection.h"
#include "../project/CG2/editor/EditorSession.h"
#include "../project/CG2/engine/core/FrameProfiler.h"
#include "PrefabFile.h"
#include "SceneFile.h"
#include "SceneObjects.h"
#include "SceneViewMath.h"
#include <stdexcept>
#include <cmath>
#include <fstream>

namespace {
	void RequireWorkflow(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}
	void VerifyMultipleSelection() {
		SceneObjects scene;
		const SceneObjectId parent = scene.CreateObject(SceneObjectType::Empty);
		const SceneObjectId child = scene.AddObject("plane.gltf");
		const SceneObjectId independent = scene.AddObject("axis.obj");
		std::string error;
		RequireWorkflow(scene.SetObjectParent(child, parent, error), "Selection parent setup failed");
		EditorSelection selection;
		selection.Select(parent);
		selection.Toggle(child);
		selection.Toggle(independent);
		RequireWorkflow(selection.Get() == independent && selection.GetIds().size() == 3, "Primary/multiple selection incorrect");
		RequireWorkflow(selection.GetRoots(scene).size() == 2, "Selected child must not be operated twice");
		EditorSession history;
		history.Initialize(scene);
		RequireWorkflow(selection.Duplicate(scene, error), "Batch duplicate failed");
		RequireWorkflow(scene.GetEditableObjectCount() == 6 && selection.GetIds().size() == 2, "Batch duplicated selected child twice");
		history.Update(scene, false);
		RequireWorkflow(history.Undo(scene, error) && scene.GetEditableObjectCount() == 3, "Batch duplicate must be one undo");
		selection.Prune(scene);
		RequireWorkflow(selection.GetIds().empty(), "Deleted selection IDs were retained");
		RequireWorkflow(history.Redo(scene, error), "Batch redo failed");
		selection.Select(parent);
		selection.Toggle(child);
		RequireWorkflow(selection.Remove(scene, error) && scene.GetEditableObjectCount() == 4, "Batch subtree removal incorrect");
		selection.Select(independent);
		selection.Toggle(independent);
		RequireWorkflow(selection.Get() == 0, "Ctrl toggle did not deselect");
		const SceneObjectId retired = scene.AddObject("axis.obj");
		RequireWorkflow(scene.RemoveObject(retired), "Retired ID removal failed");
		selection.Select(independent);
		RequireWorkflow(selection.Duplicate(scene, error) && selection.Get() > retired, "Batch duplicate reused a deleted ID");
		selection.Clear();
		// 最後の複製だけが上限を超えても、最初の複製まで元シーンへ反映しない。
		SceneObjects lightScene;
		for (size_t lightIndex = 0; lightIndex < 7; ++lightIndex) {
			selection.Toggle(lightScene.CreateObject(SceneObjectType::PointLight));
		}
		RequireWorkflow(!selection.Duplicate(lightScene, error) && lightScene.GetEditableObjectCount() == 7, "Failed batch duplicate partially changed scene");
	}

	void VerifySurfaceIntersection() {
		float distance = 0;
		RequireWorkflow(SceneViewMath::IntersectTriangle({0.25f,0.25f,-3}, {0,0,1}, {0,0,0}, {1,0,0}, {0,1,0}, distance), "Triangle hit missing");
		RequireWorkflow(std::abs(distance - 3) < 0.0001f, "Surface distance incorrect");
		RequireWorkflow(SceneViewMath::IntersectTriangle({0.25f,0.25f,3}, {0,0,-1}, {0,0,0}, {1,0,0}, {0,1,0}, distance), "Backface surface hit missing");
		RequireWorkflow(!SceneViewMath::IntersectTriangle({0.8f,0.8f,-3}, {0,0,1}, {0,0,0}, {1,0,0}, {0,1,0}, distance), "AABB empty corner must not snap");
		RequireWorkflow(!SceneViewMath::IntersectTriangle({0,0,-3}, {1,0,0}, {0,0,0}, {1,0,0}, {0,1,0}, distance), "Parallel triangle accepted");
		RequireWorkflow(!SceneViewMath::IntersectTriangle({0,0,-3}, {0,0,-1}, {0,0,0}, {1,0,0}, {0,1,0}, distance), "Triangle behind ray accepted");
		RequireWorkflow(!SceneViewMath::IntersectTriangle({0,0,-3}, {0,0,1}, {}, {}, {}, distance), "Degenerate triangle accepted");
	}

	void VerifyLinkedPrefab() {
		SceneObjects source;
		const SceneObjectId root = source.CreateObject(SceneObjectType::Empty);
		const SceneObjectId child = source.AddObject("axis.obj");
		std::string error;
		RequireWorkflow(source.SetObjectParent(child, root, error), "Linked prefab setup failed");
		RequireWorkflow(PrefabFile::Save(source, root, "resources/workflow.ponprefab", error), "Linked prefab save failed");
		SceneObjects scene;
		// 高いルートIDが再配置した子の範囲に入っても重複させない。
		SceneObjectData seed;
		seed.id = 100;
		RequireWorkflow(scene.ReplaceObjects({seed}, error), "Prefab seed failed");
		const SceneObjectId instance = scene.PlacePrefab("workflow.ponprefab", 0, {3,4,5}, error);
		RequireWorkflow(instance != 0 && !scene.GetObjectData(1)->prefabSource.empty(), "Prefab source link missing");
		const SceneObjectId second = scene.PlacePrefab("workflow.ponprefab", 0, {}, error);
		RequireWorkflow(second != 0, "Second linked instance failed");
		RequireWorkflow(SceneFile::Save(scene, "linked.ponscene", error), "Linked scene save failed");
		SceneObjects restored;
		RequireWorkflow(SceneFile::Load(restored, "linked.ponscene", error), "Linked scene load failed");
		RequireWorkflow(restored.GetObjectData(1)->prefabSource == "workflow.ponprefab", "Prefab link lost after reload");
		// 直前の保存形式7は、リンク情報なしで同じComponentを復元できる。
		std::ifstream currentFormat("linked.ponscene");
		std::ofstream previousFormat("previous-format.ponscene");
		std::string record;
		std::getline(currentFormat, record);
		previousFormat << "PON_SCENE 7\n";
		std::getline(currentFormat, record);
		previousFormat << record << '\n';
		while (std::getline(currentFormat, record)) {
			const size_t sourcePosition = record.rfind(" \"");
			RequireWorkflow(sourcePosition != std::string::npos, "Expected serialized prefab source");
			previousFormat << record.substr(0, sourcePosition) << '\n';
		}
		previousFormat.close();
		SceneObjects previousScene;
		RequireWorkflow(SceneFile::Load(previousScene, "previous-format.ponscene", error), "Format 7 backward compatibility failed");
		RequireWorkflow(previousScene.GetObjectData(1)->prefabSource.empty(), "Legacy scene inherited a prefab link");
		*scene.GetEditableObject(2).editableName = "Applied Child";
		RequireWorkflow(PrefabFile::Apply(scene, instance, error), "Prefab Apply failed");
		RequireWorkflow(PrefabFile::Revert(scene, second, error), "Other instance Revert failed");
		RequireWorkflow(scene.GetObjectData(scene.GetEditableObjectCount()-1)->name == "Applied Child", "Revert did not load applied asset");
		EditorSession history;
		history.Initialize(scene);
		*scene.GetEditableObject(2).editableName = "Unsaved Override";
		history.Update(scene, false);
		RequireWorkflow(PrefabFile::Revert(scene, instance, error), "Prefab Revert failed");
		RequireWorkflow(scene.GetObjectWorldPosition(instance).x == 3 && scene.GetObjectWorldPosition(instance).y == 4, "Revert changed root transform");
		history.Update(scene, false);
		RequireWorkflow(history.Undo(scene, error), "Prefab Revert undo failed");
		RequireWorkflow(scene.GetObjectData(2)->name == "Unsaved Override", "Prefab undo did not recover override");
		RequireWorkflow(PrefabFile::ResolveSource("../escape.ponprefab", error).empty(), "Escaping prefab source accepted");
		RequireWorkflow(!PrefabFile::Apply(scene, 99999, error), "Missing linked root accepted");
		// ソースが欠けている場合もシーンを変えない。
		const size_t previousCount = scene.GetEditableObjectCount();
		std::filesystem::rename("resources/workflow.ponprefab", "resources/missing.ponprefab");
		RequireWorkflow(!PrefabFile::Revert(scene, instance, error) && scene.GetEditableObjectCount() == previousCount, "Failed Revert partially changed scene");
	}

	void VerifyProfiler() {
		FrameProfiler::Initialize();
		for (size_t frameIndex = 0; frameIndex < 605; ++frameIndex) {
			FrameProfiler::BeginFrame();
			FrameProfiler::SetGpuMilliseconds(0.5f);
			{
				FrameProfiler::Scope section(FrameProfiler::Section::Models);
				FrameProfiler::RecordDraw(6, 10);
				FrameProfiler::RecordDraw(3, 1);
				FrameProfiler::RecordUiDraw();
			}
			FrameProfiler::EndFrame();
		}
		RequireWorkflow(FrameProfiler::GetCount() == 600 && FrameProfiler::GetSample(0).number == 6, "Profiler ring buffer incorrect");
		RequireWorkflow(FrameProfiler::GetLatest().drawCalls == 2 && FrameProfiler::GetLatest().triangles == 21 && FrameProfiler::GetLatest().instances == 11, "Draw counters confused calls/instances");
		RequireWorkflow(FrameProfiler::GetLatest().uiDrawCalls == 1, "UI counter missing");
		RequireWorkflow(FrameProfiler::GetLatest().gpuTimingValid && FrameProfiler::GetLatest().gpuMilliseconds == 0.5f, "GPU duration was not retained in frame history");
		const uint64_t latest = FrameProfiler::GetLatest().number;
		FrameProfiler::SetPaused(true);
		FrameProfiler::BeginFrame();
		FrameProfiler::EndFrame();
		RequireWorkflow(FrameProfiler::GetLatest().number == latest, "Frozen profiler history changed");
		RequireWorkflow(FrameProfiler::GetPeak().number != 0, "Peak sample was not retained");
		RequireWorkflow(FrameProfiler::ExportCsv("profiler.csv"), "CSV export failed");
		std::ifstream capture("profiler.csv");
		std::string record;
		size_t records = 0;
		while (std::getline(capture, record)) {
			if (records == 0) {
				RequireWorkflow(record.find("gpu_valid,gpu_ms") != std::string::npos, "GPU columns missing from CSV");
			}
			++records;
		}
		RequireWorkflow(records == 601, "CSV history row count incorrect");
		FrameProfiler::ResetPeak();
		RequireWorkflow(FrameProfiler::GetPeak().number == 0, "Peak reset failed");
		FrameProfiler::SetPaused(false);
		FrameProfiler::BeginFrame();
		FrameProfiler::SetGpuMilliseconds(-1);
		FrameProfiler::EndFrame();
		RequireWorkflow(!FrameProfiler::GetLatest().gpuTimingValid, "Invalid GPU time was accepted");
		FrameProfiler::BeginFrame();
		FrameProfiler::SetGpuMilliseconds(0);
		FrameProfiler::EndFrame();
		RequireWorkflow(FrameProfiler::GetLatest().gpuTimingValid && FrameProfiler::GetLatest().gpuMilliseconds == 0, "Valid zero GPU time was treated as missing");
		FrameProfiler::BeginFrame();
		FrameProfiler::EndFrame();
		RequireWorkflow(!FrameProfiler::GetLatest().gpuTimingValid, "A missing GPU sample reused the previous frame");
	}
}

void RunEditorWorkflowTests(const std::filesystem::path& directory) {
	VerifyMultipleSelection();
	VerifySurfaceIntersection();
	const std::filesystem::path previous = std::filesystem::current_path();
	const std::filesystem::path isolated = std::filesystem::absolute(directory / "editor-workflow");
	std::filesystem::create_directories(isolated / "resources");
	std::filesystem::current_path(isolated);
	VerifyLinkedPrefab();
	VerifyProfiler();
	std::filesystem::current_path(previous);
}
