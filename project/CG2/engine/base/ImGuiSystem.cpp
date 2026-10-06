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

	ImGui::StyleColorsDark();

	//=============================================================================================//
	// Win32初期化

	ImGui_ImplWin32_Init(winApp->GetHwnd());

	//=============================================================================================//
	// DX12初期化

	DescriptorHeapManager* srvHeap = dxCommon_->GetSrvHeap();

	uint32_t srvIndex = srvHeap->Allocate();

	ImGui_ImplDX12_Init(
		dxCommon_->GetDevice(),
		static_cast<int>(DirectXCommon::kBufferCount),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvHeap->GetDescriptorHeap(),
		srvHeap->GetCPUDescriptorHandle(srvIndex),
		srvHeap->GetGPUDescriptorHandle(srvIndex));

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