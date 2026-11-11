#pragma once

#include "EditorScene.h"
#include "EditorPanels.h"

// SceneとPanelsの呼び出し窓口。
class Editor {
public:
	Editor();
	Editor(const Editor&) = delete;
	Editor& operator=(const Editor&) = delete;
	void InitializeUi();
	void Initialize(IEditableWorld* scene, const std::string& initialPath = {});
	void OnSceneChanged(IEditableWorld* scene, const char* sceneName, const std::string& initialPath = {});
	void Update(IEditableWorld* scene);
	void Draw(IEditableWorld* scene);
	bool IsPlaying() const;
	bool ShouldSimulate();
	bool ShouldExit() const;
	uint32_t GetSceneWidth() const;
	uint32_t GetSceneHeight() const;
	void SetSceneTexture(uint64_t textureHandle);
	void RequestExit(IEditableWorld* scene);

private:
	EditorScene scene_;
	EditorPanels panels_;
};
