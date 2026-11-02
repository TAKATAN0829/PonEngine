#include "GameViewPanel.h"
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include <algorithm>
#endif

//=============================================================================================//
// 描画処理


void GameViewPanel::Draw(IEditableWorld& scene, uint64_t textureHandle, uint32_t& width, uint32_t& height) {
#ifdef USE_IMGUI
	// 実行用カメラの画像を表示する間は、編集カメラへ入力を渡さない。
	scene.SetEditorCameraInput(false);
	CameraView camera{};
	if (scene.GetGameCameraView(static_cast<float>(width) / static_cast<float>(height), camera)) {
		ImGui::TextUnformatted("Main Camera / Playで動作確認");
	} else {
		ImGui::TextDisabled("メインカメラ未指定・無効：既定の視点を使用中");
	}
	if (textureHandle == 0) {
		return;
	}
	const ImVec2 available = ImGui::GetContentRegionAvail();
	if (available.x < 64 || available.y < 64) {
		return;
	}
	width = static_cast<uint32_t>((std::min)(available.x, 4096.0f));
	height = static_cast<uint32_t>((std::min)(available.y, 4096.0f));
	scene.SetEditorViewport(width,height);
	scene.SetEditorGameView(true);
	ImGui::Image(ImTextureRef(static_cast<ImTextureID>(textureHandle)),
		{static_cast<float>(width),static_cast<float>(height)});
#else
	(void)scene;
	(void)textureHandle;
	(void)width;
	(void)height;
#endif
}
