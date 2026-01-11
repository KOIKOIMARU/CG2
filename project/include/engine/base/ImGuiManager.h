#pragma once
#include <cstdint>
#include <wrl.h>
#include <d3d12.h>
#include "engine/base/Math.h"

class WinApp;
class DirectXCommon;
class SrvManager;

#ifdef USE_IMGUI
// ここは君のプロジェクトの実際のパスに合わせる
#include "imgui.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx12.h"
#endif

class ImGuiManager {
public:
    void Initialize(WinApp* winApp, DirectXCommon* dxCommon, SrvManager* srvManager);
    void Begin();
    void End();
    void Draw();
    void Finalize();

    void ShowSpriteController(Math::Vector2& spritePos);

private:
    DirectXCommon* dxCommon_ = nullptr;
    uint32_t imguiSrvIndex_ = 0;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
};
