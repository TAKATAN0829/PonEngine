#include "ImGuiSystem.h"

// ImGui
#ifdef USE_IMGUI
#include "../externals/imgui/imgui.h"
#include "../externals/imgui/imgui_impl_dx12.h"
#include "../externals/imgui/imgui_impl_win32.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

//=================================================================================================//
// 初期化処理

void ImGuiSystem::Initialize(
	HWND hwnd,
	ID3D12Device* device,
	DescriptorHeapManager* srvDescriptorHeap,
	uint32_t bufferCount,
	uint32_t descriptorIndex) {

	//=============================================================================================//
	// ImGui初期化
#ifdef USE_IMGUI
	IMGUI_CHECKVERSION();

	ImGui::CreateContext();

	ImGui::StyleColorsDark();

	//=============================================================================================//
	// Win32初期化

	ImGui_ImplWin32_Init(
		hwnd);

	//=============================================================================================//
	// DX12初期化

	ImGui_ImplDX12_Init(
		device,
		static_cast<int>(bufferCount),
		DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
		srvDescriptorHeap->GetDescriptorHeap(),
		srvDescriptorHeap->GetCPUDescriptorHandle(
			descriptorIndex),
		srvDescriptorHeap->GetGPUDescriptorHandle(
			descriptorIndex));

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

void ImGuiSystem::Draw(ID3D12GraphicsCommandList* commandList) {

	if (isInitialized_ == false) {

		return;
	}

	//=============================================================================================//
	// ImGui描画
#ifdef USE_IMGUI
	ImGui::Render();

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