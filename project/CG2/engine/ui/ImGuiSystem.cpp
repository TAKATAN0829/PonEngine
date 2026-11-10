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

//=============================================================================================//
// 実装内補助定義

// Win32入力イベントを処理。
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace {
// ImGuiのテクスチャもエンジンと同じSRVヒープから確保する。
void AllocateGuiDescriptor(ImGui_ImplDX12_InitInfo* information, D3D12_CPU_DESCRIPTOR_HANDLE* cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* gpuHandle) {
	// ImGui用の記述子を、共有SRV管理から確保する。
    SrvManager* manager = static_cast<SrvManager*>(information->UserData);

    const uint32_t descriptorIndex = manager->Allocate();

    *cpuHandle = manager->GetCPUDescriptorHandle(descriptorIndex);

    *gpuHandle = manager->GetGPUDescriptorHandle(descriptorIndex);
}

// フォント再生成などで不要になった記述子は共有管理へ戻す。
void FreeGuiDescriptor(ImGui_ImplDX12_InitInfo* information, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE) {
	// ImGuiが使い終えた記述子を共有SRV管理へ返す。
    SrvManager* manager = static_cast<SrvManager*>(information->UserData);

    manager->Free(manager->GetIndex(cpuHandle));
}
}

#endif

//=============================================================================================//
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
	// コンテキストと日本語フォントを用意し、Win32・DirectX 12へ接続する。
	IMGUI_CHECKVERSION();

	// ImGuiコンテキストを作成。
	ImGui::CreateContext();
	// 入力状態を取得。
	ImGuiIO& inputOutput = ImGui::GetIO();
	// 暗色スタイルを適用。
	ImGui::StyleColorsDark();

    wchar_t windowsDirectory[MAX_PATH]{};

    // Windows のインストール先を取得し、ドライブ文字に依存せずフォントを探す。
    GetWindowsDirectoryW(windowsDirectory, MAX_PATH);

    // OS の Fonts フォルダにあるメイリオのファイルを指定する。
    const std::filesystem::path fontPath = std::filesystem::path(windowsDirectory) / "Fonts" / "meiryo.ttc";

    std::error_code fontError;

    // 日本語フォントのファイルがあるときだけ読み込み、ない環境では既定フォントを使う。
    if (std::filesystem::exists(fontPath, fontError)) {
        ImFontConfig fontConfiguration;

        fontConfiguration.SizePixels = 18;

        // ImGui にフォントファイルを読み込ませ、文字を描画するためのアトラスに登録する。
        inputOutput.Fonts->AddFontFromFileTTF(fontPath.string().c_str(), 18, &fontConfiguration);
    } else {
        inputOutput.Fonts->AddFontDefault();
    }

	//===========================================//
	// Win32初期化

	// Win32入力連携を初期化。
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

	// DX12描画を初期化。
	ImGui_ImplDX12_Init(&initInfo);

	isInitialized_ = true;

#endif
}

//=============================================================================================//
// 受付開始


void ImGuiSystem::Begin() {

	if (isInitialized_ == false) {

		return;
	}

	//===========================================//
	// ImGuiフレーム開始
#ifdef USE_IMGUI
	// Win32入力とDX12状態を更新してから、新しいUIフレームを開始する。
	// DX12描画フレームを準備。
	ImGui_ImplDX12_NewFrame();
	// Win32入力状態を更新。
	ImGui_ImplWin32_NewFrame();
	// UIフレームを開始。
	ImGui::NewFrame();

#endif
}

//=============================================================================================//
// 受付終了


void ImGuiSystem::End() {

	if (isInitialized_ == false) {

		return;
	}

	//===========================================//
	// 描画データ確定
#ifdef USE_IMGUI
	// このフレームのWidget描画を確定し、後段の描画処理へ渡す。
	// UI描画データを確定。
	ImGui::Render();

#endif
}

//=============================================================================================//
// 描画処理


void ImGuiSystem::Draw() {

	if (isInitialized_ == false) {

		return;
	}

	//=============================================
	// ImGui描画
#ifdef USE_IMGUI
	// UI用Descriptorを設定し、表示領域に残る描画命令だけを集計して描画する。
	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	ID3D12DescriptorHeap* ppHeaps[] = { srvManager_->GetDescriptorHeap() };

	commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

	// バックエンドが発行する有効なUI描画命令を別カウンターへ記録する。
	// UI描画データを取得。
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

	// UIをDX12で描画。
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);

#endif
}

//=============================================================================================//
// 終了処理


void ImGuiSystem::Finalize() {

	if (isInitialized_ == false) {

		return;
	}

	//===========================================//
	// ImGui終了
#ifdef USE_IMGUI
	// DX12・Win32バックエンドを停止し、ImGuiコンテキストを破棄する。
	// DX12描画連携を終了。
	ImGui_ImplDX12_Shutdown();
	// Win32入力連携を終了。
	ImGui_ImplWin32_Shutdown();
	// ImGuiコンテキストを破棄。
	ImGui::DestroyContext();

	isInitialized_ = false;

#endif
}
