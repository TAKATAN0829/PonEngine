#pragma once
class Editor {
public:
	void Initialize();

	void Update();

	void Draw();

private:
	enum class PanelType {
		Hierarchy,
		Inspector,
		Console,
		Project,
		SceneView,
	};
};

