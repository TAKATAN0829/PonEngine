#include "ImGuiSystem.h"

// C++
#include <cassert>

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

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

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

	initInfo.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {

			SrvManager* srvManager = static_cast<SrvManager*>(info->UserData);

			uint32_t index = srvManager->Allocate();

			*outCpuHandle = srvManager->GetCPUDescriptorHandle(index);
			*outGpuHandle = srvManager->GetGPUDescriptorHandle(index);
		};

	initInfo.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo* info, D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle, D3D12_GPU_DESCRIPTOR_HANDLE) {

			SrvManager* srvManager = static_cast<SrvManager*>(info->UserData);

			srvManager->Free(srvManager->GetIndex(cpuHandle));
		};

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

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
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