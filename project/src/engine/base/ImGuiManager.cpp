#include "engine/base/ImGuiManager.h"
#include "engine/base/WinApp.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"

#include <cassert>

namespace {

void AllocateImGuiSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE* outCpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE* outGpuHandle)
{
    auto* srvManager = static_cast<SrvManager*>(info->UserData);
    assert(srvManager);

    uint32_t srvIndex = srvManager->Allocate();
    *outCpuHandle = srvManager->GetCPUDescriptorHandle(srvIndex);
    *outGpuHandle = srvManager->GetGPUDescriptorHandle(srvIndex);
}

void FreeImGuiSrvDescriptor(
    ImGui_ImplDX12_InitInfo* info,
    D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle,
    D3D12_GPU_DESCRIPTOR_HANDLE)
{
    auto* srvManager = static_cast<SrvManager*>(info->UserData);
    assert(srvManager);

    // ImGuiが不要になったSRVを、次の確保で再利用できるように戻す
    srvManager->Free(cpuHandle);
}

}

void ImGuiManager::Initialize(
    [[maybe_unused]] WinApp* winApp,
    [[maybe_unused]] DirectXCommon* dxCommon,
    [[maybe_unused]] SrvManager* srvManager)
{
#ifdef USE_IMGUI
    assert(winApp);
    assert(dxCommon);
    assert(srvManager);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplWin32_Init(winApp->GetHwnd());

    srvHeap_ = srvManager->GetDescriptorHeapComPtr();

    ImGui_ImplDX12_InitInfo initInfo{};
    initInfo.Device = dxCommon_->GetDevice();
    initInfo.CommandQueue = dxCommon_->GetCommandQueue();
    initInfo.NumFramesInFlight =
        static_cast<int>(dxCommon_->GetSwapChainResourcesNum());
    initInfo.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    initInfo.SrvDescriptorHeap = srvHeap_.Get();
    initInfo.SrvDescriptorAllocFn = AllocateImGuiSrvDescriptor;
    initInfo.SrvDescriptorFreeFn = FreeImGuiSrvDescriptor;
    initInfo.UserData = srvManager_;

    ImGui_ImplDX12_Init(&initInfo);
#endif
}

void ImGuiManager::Begin() {
#ifdef USE_IMGUI
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
#endif
}

void ImGuiManager::End() {
#ifdef USE_IMGUI
    ImGui::Render();
#endif
}

void ImGuiManager::Draw() {
#ifdef USE_IMGUI
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();
    assert(commandList);

    ID3D12DescriptorHeap* ppHeaps[] = { srvHeap_.Get() };
    commandList->SetDescriptorHeaps(_countof(ppHeaps), ppHeaps);

    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);
#endif
}

void ImGuiManager::Finalize() {
#ifdef USE_IMGUI
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
#endif
}

void ImGuiManager::ShowSpriteController(Math::Vector2& spritePos) {
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(500, 100), ImGuiCond_Always);
    ImGui::Begin("Sprite Controller", nullptr,
        ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse);

    ImGui::SliderFloat("X", &spritePos.x, 0.0f, 1280.0f);
    ImGui::SliderFloat("Y", &spritePos.y, 0.0f, 720.0f);
    ImGui::Text("Pos : %07.1f , %07.1f", spritePos.x, spritePos.y);

    ImGui::End();
#else
    (void)spritePos;
#endif
}

void ImGuiManager::ShowGamePlayController(
    Math::Vector3& objectRotate,
    Math::Vector3& lightDirection,
    float& lightIntensity)
{
#ifdef USE_IMGUI
    ImGui::SetNextWindowSize(ImVec2(500, 180), ImGuiCond_Once);
    ImGui::Begin("GamePlay Controller", nullptr,
        ImGuiWindowFlags_NoCollapse);

    // Planeの回転を調整
    ImGui::Text("Object Rotate");
    ImGui::SliderFloat("Rotate X", &objectRotate.x, -3.14f, 3.14f);
    ImGui::SliderFloat("Rotate Y", &objectRotate.y, -3.14f, 3.14f);
    ImGui::SliderFloat("Rotate Z", &objectRotate.z, -3.14f, 3.14f);

    ImGui::Separator();

    // DirectionalLightの向きと強さを調整
    ImGui::Text("Directional Light");
    ImGui::SliderFloat("Light X", &lightDirection.x, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Y", &lightDirection.y, -1.0f, 1.0f);
    ImGui::SliderFloat("Light Z", &lightDirection.z, -1.0f, 1.0f);
    ImGui::SliderFloat("Intensity", &lightIntensity, 0.0f, 5.0f);

    ImGui::End();
#else
    (void)objectRotate;
    (void)lightDirection;
    (void)lightIntensity;
#endif
}
