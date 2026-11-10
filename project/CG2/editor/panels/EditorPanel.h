#pragma once

// ImGuiのWindowを確実に閉じるための、画面パネル共通基底。
class EditorPanel {
protected:
	class Window {
	public:
		Window(const EditorPanel& panel, bool* isOpen = nullptr, int flags = 0);
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		bool IsVisible() const;
		void Close();

	private:
		bool isVisible_ = true;
		bool isOpen_ = false;
	};

	explicit EditorPanel(const char* windowName);

private:
	const char* windowName_;
};
