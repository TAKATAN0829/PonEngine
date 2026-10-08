#include "ImGuiSystem.h"

// C++
#include <cassert>

// 自作
#include "WinApp.h"
#include "DirectXCommon.h"

// ImGui
#ifdef USE_IMGUI
#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx12.h"
#include "imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND hwnd,
	UINT msg,
	WPARAM wParam,
	LPARAM lParam);
#endif

//=================================================================================================//
// 初期化処理

void ImGuiSystem::Initialize(WinApp* winApp, DirectXCommon* dxCommon) {

	assert(winApp);
	assert(dxCommon);

	dxCommon_ = dxCommon;

	//=============================================================================================//
	// ImGui初期化
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();

	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	//=============================================================================================//
	// Win32初期化

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	//=============================================================================================//
	// DX12初期化

	ImGui_ImplDX12_InitInfo initInfo{};

	initInfo.Device = dxCommon_->GetDevice();
	initInfo.CommandQueue = dxCommon_->GetCommandQueue();
	initInfo.NumFramesInFlight = static_cast<int>(DirectXCommon::kBufferCount);
	initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	initInfo.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	initInfo.UserData = dxCommon_->GetSrvHeap();
	initInfo.SrvDescriptorHeap = dxCommon_->GetSrvHeap()->GetDescriptorHeap();

	initInfo.SrvDescriptorAllocFn = [](
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle) {

			DescriptorHeapManager* srvHeap = static_cast<DescriptorHeapManager*>(info->UserData);

			uint32_t index = srvHeap->Allocate();

			*outCpuHandle = srvHeap->GetCPUDescriptorHandle(index);
			*outGpuHandle = srvHeap->GetGPUDescriptorHandle(index);
		};

	initInfo.SrvDescriptorFreeFn = [](
		ImGui_ImplDX12_InitInfo* info,
		D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
		D3D12_GPU_DESCRIPTOR_HANDLE) {

			DescriptorHeapManager* srvHeap = static_cast<DescriptorHeapManager*>(info->UserData);

			srvHeap->Free(srvHeap->GetIndex(cpuHandle));
		};

	ImGui_ImplDX12_Init(&initInfo);

	isInitialized_ = true;
#endif
}

//=================================================================================================//
// 開始処理

void ImGuiSystem::Begin() {

	if (isInitialized_ == false) {

		return;
	}

	//=============================================================================================//
	// ImGuiフレーム開始
#ifdef USE_IMGUI
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
#endif
}

//=================================================================================================//
// 描画処理

void ImGuiSystem::Draw() {

	if (isInitialized_ == false) {

		return;
	}

	//=============================================================================================//
	// ImGui描画
#ifdef USE_IMGUI
	ImGui::Render();

	ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

	ID3D12DescriptorHeap* descriptorHeaps[] = { dxCommon_->GetSrvHeap()->GetDescriptorHeap() };

	commandList->SetDescriptorHeaps(1, descriptorHeaps);

	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

//=================================================================================================//
// 終了処理

void ImGuiSystem::Finalize() {

	if (isInitialized_ == false) {

		return;
	}

	//=============================================================================================//
	// ImGui終了
#ifdef USE_IMGUI
	ImGui_ImplDX12_Shutdown();

	ImGui_ImplWin32_Shutdown();

	ImGui::DestroyContext();

	isInitialized_ = false;
#endif
}