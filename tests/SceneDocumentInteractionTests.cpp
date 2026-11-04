#include "SceneDocumentInteractionTests.h"
#include "../project/CG2/editor/Editor.h"
#include "objects/WorldObjects.h"
#include "serialization/SceneFile.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <chrono>
#include <stdexcept>

namespace {
	void RequireSceneUi(bool condition, const char* message) {
		if (!condition) {
			throw std::runtime_error(message);
		}
	}

	class SceneUiContext {
	public:
		SceneUiContext() : previous_(ImGui::GetCurrentContext()) {
			context_ = ImGui::CreateContext();

			ImGui::SetCurrentContext(context_);

			ImGuiIO& input = ImGui::GetIO();

			input.IniFilename = nullptr;

			input.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

			input.ConfigInputTrickleEventQueue = false;

			input.DisplaySize = {1200, 900};

			input.DeltaTime = 1.0f / 60;

			input.Fonts->AddFontDefault();

			input.Fonts->Build();
		}

		~SceneUiContext() {
			ImGui::DestroyContext(context_);

			ImGui::SetCurrentContext(previous_);
		}

	private:
		ImGuiContext* previous_ = nullptr;
		ImGuiContext* context_ = nullptr;
	};

	class SceneUiWorld : public WorldObjects {
	public:
		ParticleSettings* GetParticleSettings() override { return &shared; }

		void OnSceneContentsChanged() override { ++changes; }

		ParticleSettings shared;

		int changes = 0;
	};

	class SceneUiFixture {
	public:
		SceneUiFixture() {
			directory = std::filesystem::path("scenes") / ("scene-ui-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));

			world.AddObject("plane.gltf");

			world.GetEditableObject(0).transform->translate.x = 1;

			editor.Initialize(&world);

			Draw();

			Draw();
		}

		void Draw() {
			ImGui::NewFrame();

			editor.Draw(&world);

			editor.Update(&world);

			ImGui::EndFrame();
		}

		void Shortcut(ImGuiKey key, bool shift = false) {
			Draw();

			Draw();

			ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);

			ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, shift);

			ImGui::GetIO().AddKeyEvent(key, true);

			Draw();

			ImGui::GetIO().AddKeyEvent(key, false);

			ImGui::GetIO().AddKeyEvent(ImGuiMod_Shift, false);

			ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);

			Draw();

			Draw();
		}

		void EnterFilename(const std::filesystem::path& path) {
			RequireSceneUi(PopupActive("Sceneファイル"), "Scene file picker did not open.");

			ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, true);

			ImGui::GetIO().AddKeyEvent(ImGuiKey_A, true);

			Draw();

			ImGui::GetIO().AddKeyEvent(ImGuiKey_A, false);

			ImGui::GetIO().AddKeyEvent(ImGuiMod_Ctrl, false);

			Draw();

			ImGui::GetIO().AddInputCharactersUTF8(AssetPathChange::Text(path).c_str());

			Draw();

			ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, true);

			Draw();

			ImGui::GetIO().AddKeyEvent(ImGuiKey_Enter, false);

			Draw();

			Draw();
		}

		bool PopupActive(const char* name) const {
			const ImGuiWindow* window = ImGui::FindWindowByName(name);

			if (window == nullptr) {
				return false;
			}

			return window->Active;
		}

		void Activate(const char* popupName, const char* button) {
			ImGuiWindow* window = ImGui::FindWindowByName(popupName);

			RequireSceneUi(window != nullptr && window->Active, "Expected scene confirmation popup is not active.");

			ImGui::ActivateItemByID(window->GetID(button));

			Draw();

			Draw();
		}

		void DoubleClickFirstScene() {
			ImGui::SetWindowFocus("Project");

			Draw();

			Draw();

			ImGuiWindow* grid = nullptr;

			for (ImGuiWindow* window : ImGui::GetCurrentContext()->Windows) {
				if (std::string(window->Name).find("/AssetGrid_") != std::string::npos) {
					if (window->Active) {
						grid = window;
					}
				}
			}

			RequireSceneUi(grid != nullptr, "Project asset grid is not active.");

			const ImVec2 position = {grid->DC.CursorStartPos.x + 40, grid->DC.CursorStartPos.y + 40};

			ImGui::GetIO().AddMousePosEvent(position.x, position.y);

			Draw();

			for (int click = 0; click < 2; ++click) {
				ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, true);

				Draw();

				ImGui::GetIO().AddMouseButtonEvent(ImGuiMouseButton_Left, false);

				Draw();
			}

			Draw();
		}

		std::filesystem::path Absolute(const char* name) const {
			return std::filesystem::canonical("resources") / directory / name;
		}

		SceneUiContext context;

		SceneUiWorld world;

		Editor editor;

		std::filesystem::path directory;
	};
}

void RunSceneDocumentInteractionTests() {
	SceneUiFixture fixture;

	fixture.Shortcut(ImGuiKey_S);

	fixture.EnterFilename(fixture.directory / "Alpha.ponscene");

	RequireSceneUi(fixture.editor.GetScenePath() == fixture.Absolute("Alpha.ponscene"), "Untitled save did not select Alpha.");

	fixture.world.GetEditableObject(0).transform->translate.x = 8;

	fixture.Draw();

	fixture.Shortcut(ImGuiKey_S, true);

	fixture.EnterFilename(fixture.directory / "Beta.ponscene");

	RequireSceneUi(fixture.editor.GetScenePath() == fixture.Absolute("Beta.ponscene"), "Save As did not select Beta.");

	fixture.Shortcut(ImGuiKey_Z);

	RequireSceneUi(fixture.world.GetObjectData(0)->transform.translate.x == 1, "Save As discarded Undo history.");

	fixture.Shortcut(ImGuiKey_Y);

	RequireSceneUi(fixture.world.GetObjectData(0)->transform.translate.x == 8, "Save As discarded Redo history.");

	fixture.world.GetEditableObject(0).transform->translate.x = 12;

	fixture.Shortcut(ImGuiKey_S);

	WorldObjects saved;

	std::string error;

	RequireSceneUi(SceneFile::Load(saved, fixture.Absolute("Beta.ponscene"), error), "Beta save cannot be loaded.");

	RequireSceneUi(saved.GetObjectData(0)->transform.translate.x == 12, "Ctrl+S did not use the current filename.");

	fixture.world.GetEditableObject(0).transform->translate.x = 13;

	fixture.Draw();

	fixture.DoubleClickFirstScene();

	RequireSceneUi(fixture.PopupActive("Unsaved Scene"), "Project double-click bypassed unsaved confirmation.");

	fixture.Activate("Unsaved Scene", "Cancel");

	RequireSceneUi(fixture.editor.GetScenePath() == fixture.Absolute("Beta.ponscene") && fixture.world.GetObjectData(0)->transform.translate.x == 13,
		"Cancel changed the current document or placement.");

	fixture.Shortcut(ImGuiKey_O);

	fixture.EnterFilename(fixture.directory / "Alpha.ponscene");

	fixture.Activate("Unsaved Scene", "Save and Continue");

	RequireSceneUi(fixture.editor.GetScenePath() == fixture.Absolute("Alpha.ponscene") && fixture.world.GetObjectData(0)->transform.translate.x == 1,
		"Save and Continue did not open Alpha.");

	RequireSceneUi(SceneFile::Load(saved, fixture.Absolute("Beta.ponscene"), error) && saved.GetObjectData(0)->transform.translate.x == 13,
		"Save and Continue did not save Beta first.");

	fixture.world.GetEditableObject(0).transform->translate.x = 77;

	fixture.Draw();

	fixture.Shortcut(ImGuiKey_O);

	fixture.EnterFilename(fixture.directory / "Beta.ponscene");

	fixture.Activate("Unsaved Scene", "Discard and Continue");

	RequireSceneUi(fixture.editor.GetScenePath() == fixture.Absolute("Beta.ponscene") && fixture.world.GetObjectData(0)->transform.translate.x == 13,
		"Discard did not load Beta.");

	fixture.Shortcut(ImGuiKey_Z);

	RequireSceneUi(fixture.world.GetObjectData(0)->transform.translate.x == 13, "Undo crossed a scene boundary.");

	fixture.world.shared.spawnCount = 7;

	fixture.Shortcut(ImGuiKey_N);

	RequireSceneUi(fixture.editor.GetScenePath().empty() && fixture.world.GetEditableObjectCount() == 0, "New scene retained the previous filename or objects.");

	fixture.editor.RequestExit(&fixture.world);

	fixture.Draw();

	RequireSceneUi(!fixture.editor.ShouldExit() && fixture.PopupActive("Unsaved Scene"), "Scene switching lost the unsaved shared particle preset.");

	fixture.Activate("Unsaved Scene", "Cancel");

	fixture.world.AddObject("axis.obj");

	fixture.Draw();

	fixture.Shortcut(ImGuiKey_O);

	fixture.EnterFilename(fixture.directory / "Alpha.ponscene");

	fixture.Activate("Unsaved Scene", "Save and Continue");

	RequireSceneUi(fixture.PopupActive("Sceneファイル"), "Untitled Save and Continue did not request a filename.");

	fixture.Activate("Sceneファイル", "キャンセル");

	RequireSceneUi(fixture.PopupActive("Unsaved Scene") && fixture.editor.GetScenePath().empty(), "Cancelling Save As did not return to the pending confirmation.");

	fixture.Activate("Unsaved Scene", "Discard and Continue");

	RequireSceneUi(fixture.editor.GetScenePath() == fixture.Absolute("Alpha.ponscene"), "Cancelled Save As lost the pending scene path.");
}
