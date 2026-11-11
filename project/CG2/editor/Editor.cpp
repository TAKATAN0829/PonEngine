#include "Editor.h"

// Editor

Editor::Editor() : panels_(scene_) {
}

void Editor::InitializeUi() {
	panels_.InitializeUi();
}
void Editor::Initialize(IEditableWorld* world, const std::string& initialPath) {
	scene_.Initialize(world, initialPath);
	panels_.Initialize();
}
void Editor::OnSceneChanged(IEditableWorld* world, const char* sceneName, const std::string& initialPath) {
	panels_.OnSceneChanged(world, sceneName, initialPath);
}
void Editor::Update(IEditableWorld* world) {
	panels_.Update(world);
}
void Editor::Draw(IEditableWorld* world) {
	panels_.Draw(world);
}
bool Editor::IsPlaying() const {
	return scene_.IsPlaying();
}
bool Editor::ShouldSimulate() {
	return scene_.ShouldSimulate();
}
bool Editor::ShouldExit() const {
	return panels_.ShouldExit();
}
uint32_t Editor::GetSceneWidth() const {
	return panels_.GetSceneWidth();
}
uint32_t Editor::GetSceneHeight() const {
	return panels_.GetSceneHeight();
}
void Editor::SetSceneTexture(uint64_t textureHandle) {
	panels_.SetSceneTexture(textureHandle);
}
void Editor::RequestExit(IEditableWorld* world) {
	panels_.RequestExit(world);
}
