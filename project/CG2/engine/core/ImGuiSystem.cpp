#include "FrameProfiler.h"
#include "ImGuiSystem.h"

// C++
#include <cassert>
#include <filesystem>

// 自作
#include "WinApp.h"
#include "DirectXCommon.h"
#include "SrvManager.h"

// ImGui
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
namespace {
// ImGuiのテクスチャもエンジンと同じSRVヒープから確保する。
void AllocateGuiDescriptor(ImGui_ImplDX12_InitInfo* information, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle) {
    SrvManager* manager = static_cast<SrvManager*>(information->UserData);
    const uint32_t descriptorIndex = manager->Allocate();
    *cpuHandle = manager->GetCPUDescriptorHandle(descriptorIndex);
    *gpuHandle = manager->GetGPUDescriptorHandle(descriptorIndex);
}
// フォント再生成などで不要になった記述子は共有管理へ戻す。
void FreeGuiDescriptor(ImGui_ImplDX12_InitInfo* information, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE) {
    SrvManager* manager = static_cast<SrvManager*>(information->UserData);
    manager->Free(manager->GetIndex(cpuHandle));
}
}
#endif

//=================================================================================================//
// 初期化処理

void ImGuiSystem::Initialize([[maybe_unused]] WinApp* winApp, [[maybe_unused]] DirectXCommon* dxCommon, [[maybe_unused]] SrvManager* srvManager) {

	assert(winApp);
	assert(dxCommon);
	assert(srvManager);

	dxCommon_ = dxCommon;

	srvManager_ = srvManager;

	//===========================================//
	// ImGui初期化
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();

	ImGui::CreateContext();

	ImGuiIO& inputOutput = ImGui::GetIO();
	inputOutput.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();
    // 以前の自由配置と混ざらない専用設定を使い、ウィンドウ位置は次回も復元する。
    inputOutput.IniFilename = "editor-layout.ini";
    inputOutput.ConfigWindowsMoveFromTitleBarOnly = true;
    wchar_t windowsDirectory[MAX_PATH]{};
    GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
    const std::filesystem::path fontPath = std::filesystem::path(windowsDirectory) / "Fonts" / "meiryo.ttc";
    std::error_code fontError;
    if (std::filesystem::exists(fontPath, fontError)) {
        ImFontConfig fontConfiguration;
        fontConfiguration.SizePixels = 18;
        inputOutput.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 18, &fontConfiguration);
    } else {
        inputOutput.Fonts->AddFontDefault();
    }
    // 文字と背景のコントラストを保ち、項目間を詰めすぎない配色・余白にする。
    ImGuiStyle& style = ImGui::GetStyle();
    style.FontScaleMain = 0.85f; // 初回フレームも含め、Editor全体で倍率を固定する。
    style.WindowPadding = {12, 10};
    style.FramePadding = {8, 5};
    style.ItemSpacing = {9, 8};
    style.WindowRounding = 4;
    style.FrameRounding = 4;
    style.TabRounding = 4;
    style.ScrollbarSize = 15;
    style.Colors[ImGuiCol_WindowBg] = {0.085f, 0.095f, 0.12f, 1};
    style.Colors[ImGuiCol_Text] = {0.92f, 0.94f, 0.97f, 1};
    style.Colors[ImGuiCol_TextDisabled] = {0.52f, 0.58f, 0.65f, 1};
    style.Colors[ImGuiCol_FrameBg] = {0.14f, 0.17f, 0.21f, 1};
    style.Colors[ImGuiCol_Header] = {0.16f, 0.25f, 0.36f, 1};
    style.Colors[ImGuiCol_Button] = {0.17f, 0.28f, 0.42f, 1};
    style.Colors[ImGuiCol_ButtonHovered] = {0.23f, 0.42f, 0.62f, 1};
    // タブ選択やフォーカスでバーを発光させず、落ち着いた同じ背景を保つ。
    style.Colors[ImGuiCol_Tab] = {0.12f, 0.14f, 0.17f, 1};
    style.Colors[ImGuiCol_TabHovered] = style.Colors[ImGuiCol_Tab];
    style.Colors[ImGuiCol_TabSelected] = style.Colors[ImGuiCol_Tab];
    style.Colors[ImGuiCol_TabDimmed] = style.Colors[ImGuiCol_Tab];
    style.Colors[ImGuiCol_TabDimmedSelected] = style.Colors[ImGuiCol_Tab];
    style.Colors[ImGuiCol_TabSelectedOverline] = {0, 0, 0, 0};
    style.Colors[ImGuiCol_TabDimmedSelectedOverline] = {0, 0, 0, 0};
    style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_TitleBg];

	//===========================================//
	// Win32初期化

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	//===========================================//
	// DX12初期化

	ImGui_ImplDX12_InitInfo initInfo{};

	initInfo.Device = dxCommon_->GetDevice();
	initInfo.CommandQueue = dxCommon_->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(DirectXCommon::kBufferCount);
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.UserData = srvManager;
	initInfo.SrvDescriptorHeap = srvManager->GetDescriptorHeap();

	initInfo.SrvDescriptorAllocFn = AllocateGuiDescriptor;
	initInfo.SrvDescriptorFreeFn = FreeGuiDescriptor;

	ImGui_ImplDX12_Init(&initInfo);

	isInitialized_ = true;
#endif
}

//=================================================================================================//
// 受付開始

void ImGuiSystem::Begin() {

	if (isInitialized_ == false) {

		return;
	}

	//===========================================//
	// ImGuiフレーム開始
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// EditorLayoutが中央・左右・下部のドッキング領域を構築する。
#endif
}

//=================================================================================================//
// 受付終了

void ImGuiSystem::End() {

	if (isInitialized_ == false) {

		return;
	}

	//===========================================//
	// 描画データ確定
#ifdef USE_IMGUI
	ImGui::Render();
#endif
}

//=================================================================================================//
// 描画処理

void ImGuiSystem::Draw() {

	if (isInitialized_ == false) {

		return;
	}

	//=============================================
	// ImGui描画
#ifdef USE_IMGUI
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	ID3D12DescriptorHeap* ppHeaps[] = { srvManager_->GetDescriptorHeap() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// バックエンドが発行する有効なUI描画命令を別カウンターへ記録する。
	const ImDrawData* drawData = ImGui::GetDrawData();
	for (int listIndex = 0; listIndex < drawData->CmdListsCount; ++listIndex) {
		const ImDrawList* drawList = drawData->CmdLists[listIndex];
		for (const ImDrawCmd& command : drawList->CmdBuffer) {
			if (command.UserCallback != nullptr || command.ElemCount == 0) {
				continue;
			}
			// バックエンドと同じく、表示領域でクリップされた命令を除外する。
			const float left = (std::max)(0.0f, (command.ClipRect.x - drawData->DisplayPos.x) * drawData->FramebufferScale.x);
			const float top = (std::max)(0.0f, (command.ClipRect.y - drawData->DisplayPos.y) * drawData->FramebufferScale.y);
			const float right = (std::min)(drawData->DisplaySize.x * drawData->FramebufferScale.x, (command.ClipRect.z - drawData->DisplayPos.x) * drawData->FramebufferScale.x);
			const float bottom = (std::min)(drawData->DisplaySize.y * drawData->FramebufferScale.y, (command.ClipRect.w - drawData->DisplayPos.y) * drawData->FramebufferScale.y);
			if (right > left && bottom > top) {
				FrameProfiler::RecordUiDraw();
			}
		}
	}
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

//=================================================================================================//
// 終了処理

void ImGuiSystem::Finalize() {

	if (isInitialized_ == false) {

		return;
	}

	//===========================================//
	// ImGui終了
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();

	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	isInitialized_ = false;
#endif
}
