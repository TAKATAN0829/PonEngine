#include "../project/CG2/editor/scene/SceneDocument.h"
#include "../project/CG2/editor/assets/AssetFileOperations.h"
#include "../project/CG2/editor/EditorSession.h"
#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include <Windows.h>
#include <chrono>
#include <fstream>
#include <stdexcept>

namespace {
	void RequireDocument(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	std::string ReadDocumentBytes(const std::filesystem::path& path) {
		std::ifstream input(path, std::ios::binary);

		return std::string(std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>());
	}

	class DocumentWorld : public WorldObjects {
	public:
		void OnSceneContentsChanged() override {
			++resets;

			runtimeValue = 0;
		}

		int resets = 0;
		int runtimeValue = 12;
	};
}

// resources以外へ書き込まず、各実行で別のディレクトリを使う。
void RunSceneDocumentTests(const std::filesystem::path& directory) {
	const std::filesystem::path resources = std::filesystem::absolute(directory / "scene-documents" /
		std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) / "resources");

	std::filesystem::create_directories(resources);

	DocumentWorld world;

	world.AddObject("plane.gltf");

	world.GetEditableObject(0).transform->translate.x = 5;

	SceneDocument document;

	std::string error;

	RequireDocument(document.Initialize(resources, {}, error), "Scene document initialization failed.");

	RequireDocument(!document.HasPath() && document.GetName() == "Untitled", "New document retained a filename.");

	RequireDocument(!document.Save(world, error), "Untitled document was silently saved to a default filename.");

	RequireDocument(document.SaveAs(world, "scenes/Alpha", false, error), "Save As with extension completion failed.");

	const std::filesystem::path alpha = resources / "scenes/Alpha.ponscene";

	RequireDocument(document.GetPath() == alpha && document.GetName() == "Alpha", "Save As did not bind its filename.");

	const std::string alphaBytes = ReadDocumentBytes(alpha);

	EditorSession history;

	history.Initialize(world);

	world.GetEditableObject(0).transform->translate.x = 10;

	history.Update(world, false);

	RequireDocument(document.SaveAs(world, "scenes/Beta.ponscene", false, error), "Second Scene Save As failed.");

	history.MarkSaved(world);

	const std::filesystem::path beta = document.GetPath();

	RequireDocument(history.CanUndo(), "Save As discarded the existing Undo history.");

	RequireDocument(history.Undo(world, error), "Undo after Save As failed.");

	RequireDocument(world.GetObjectData(0)->transform.translate.x == 5, "Save As changed Undo data.");

	RequireDocument(ReadDocumentBytes(alpha) == alphaBytes, "Save As overwrote the previous scene.");

	RequireDocument(!document.SaveAs(world, alpha, false, error), "An unconfirmed overwrite was accepted.");

	RequireDocument(document.GetPath() == beta && ReadDocumentBytes(alpha) == alphaBytes, "Rejected overwrite changed the filename or data.");

	world.GetEditableObject(0).transform->translate.x = 20;

	RequireDocument(document.Save(world, error), "Save to the current filename failed.");

	WorldObjects saved;

	RequireDocument(SceneFile::Load(saved, beta, error), "Current scene save cannot be loaded.");

	RequireDocument(saved.GetObjectData(0)->transform.translate.x == 20, "Save used the wrong document filename.");

	RequireDocument(ReadDocumentBytes(alpha) == alphaBytes, "Saving Beta changed Alpha.");

	RequireDocument(document.Open(world, alpha, error), "Open another scene failed.");

	RequireDocument(world.GetObjectData(0)->transform.translate.x == 5 && document.GetPath() == alpha, "Opening Alpha retained Beta's placement or filename.");

	RequireDocument(world.resets == 1 && world.runtimeValue == 0, "Opening a scene did not reset transient state.");

	RequireDocument(document.New(world, error), "New document failed.");

	RequireDocument(!document.HasPath() && world.GetEditableObjectCount() == 0 && world.resets == 2, "New document inherited previous contents or filename.");

	world.AddObject("axis.obj");

	RequireDocument(!document.Save(world, error), "A new document overwrote the previously opened scene.");

	RequireDocument(ReadDocumentBytes(alpha) == alphaBytes, "New document damaged the old scene file.");

	RequireDocument(document.Open(world, beta, error), "Reopening Beta failed.");

	const int resetsBeforeFailure = world.resets;

	RequireDocument(!document.Open(world, "scenes/Missing.ponscene", error), "Missing scene was opened.");

	RequireDocument(document.GetPath() == beta && world.resets == resetsBeforeFailure, "Failed open reset the active document.");

	const HANDLE locked = CreateFileW(alpha.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);

	RequireDocument(locked != INVALID_HANDLE_VALUE, "Cannot lock isolated scene fixture.");

	const bool savedWhileLocked = document.SaveAs(world, alpha, true, error);

	CloseHandle(locked);

	RequireDocument(!savedWhileLocked && document.GetPath() == beta, "Failed Save As changed the active filename.");

	RequireDocument(ReadDocumentBytes(alpha) == alphaBytes, "Failed Save As damaged existing file bytes.");

	AssetFileOperations operations;

	RequireDocument(operations.Initialize(resources, error), "Cannot prepare active scene asset operations.");

	std::filesystem::path trash;

	const std::vector<AssetReference> activeReference = {{"scenes/Beta.ponscene", "Editor / open scene document"}};

	RequireDocument(!operations.MoveToTrash(beta, activeReference, trash, error), "Active scene asset was deleted.");

	RequireDocument(operations.Move(beta, resources / "scenes/Renamed.ponscene", world, activeReference, error), "Active scene asset could not be renamed.");

	document.RemapAssetPaths({resources, "scenes/Beta.ponscene", "scenes/Renamed.ponscene"});

	RequireDocument(document.GetName() == "Renamed", "Active document did not follow an asset rename.");

	RequireDocument(document.Save(world, error) && !std::filesystem::exists(beta), "Save recreated the old scene filename after rename.");

	RequireDocument(document.SaveAs(world, AssetPathChange::Path("scenes/タイトル.ponscene"), false, error), "Unicode scene Save As failed.");

	RequireDocument(document.GetName() == "タイトル", "Unicode scene display name was corrupted.");

	const std::filesystem::path current = document.GetPath();

	RequireDocument(!document.SaveAs(world, resources.parent_path() / "outside.ponscene", false, error), "Scene save escaped resources.");

	RequireDocument(!document.SaveAs(world, "scenes/Bad.txt", false, error), "Invalid scene extension was accepted.");

	RequireDocument(!document.SaveAs(world, "scenes/NUL", false, error), "Reserved Windows filename was accepted.");

	RequireDocument(!document.SaveAs(world, ".editor-trash/Hidden.ponscene", false, error), "Scene save wrote into the asset trash.");

	std::ofstream incomplete(resources / "scenes/Incomplete.ponscene.tmp");

	incomplete << "recoverable bytes";

	incomplete.close();

	RequireDocument(!document.SaveAs(world, "scenes/Incomplete.ponscene", false, error), "Save overwrote a recoverable temporary file.");

	RequireDocument(ReadDocumentBytes(resources / "scenes/Incomplete.ponscene.tmp") == "recoverable bytes", "Temporary recovery bytes were damaged.");

	std::ofstream broken(resources / "scenes/Broken.ponscene");

	broken << "PON_SCENE 99\n0\n";

	broken.close();

	RequireDocument(!document.Open(world, "scenes/Broken.ponscene", error), "Broken scene was opened.");

	RequireDocument(document.GetPath() == current && world.GetObjectData(0)->transform.translate.x == 20, "Broken scene changed current filename or placement.");

	RequireDocument(document.SaveAs(world, alpha, true, error), "Confirmed overwrite failed.");

	RequireDocument(document.GetPath() == alpha, "Confirmed overwrite did not bind the selected filename.");
}
