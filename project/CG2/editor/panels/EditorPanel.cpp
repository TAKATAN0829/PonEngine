#include "EditorPanel.h"

#ifdef USE_IMGUI
#include "imgui/imgui.h"
#endif

EditorPanel::EditorPanel(const char* windowName)
	: windowName_(windowName) {
}

EditorPanel::Window::Window(const EditorPanel& panel, bool* isOpen, int flags) {
#ifdef USE_IMGUI
	isVisible_ = ImGui::Begin(panel.windowName_, isOpen, flags);
	isOpen_ = true;
#else
	(void)panel;
	(void)isOpen;
	(void)flags;
#endif
}

EditorPanel::Window::~Window() {
	Close();
}

void EditorPanel::Window::Close() {
#ifdef USE_IMGUI
	if (isOpen_) {
		ImGui::End();
		isOpen_ = false;
	}
#endif
}

bool EditorPanel::Window::IsVisible() const {
	return isVisible_;
}
