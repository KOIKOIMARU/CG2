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
    void ShowGamePlayController(
        Math::Vector3& objectRotate,
        Math::Vector3& lightDirection,
        float& lightIntensity,
        int& blendModeIndex,
        float& environmentCoefficient,
        bool& showSkybox,
        int& postEffectMode,
        bool& showPlane,
        bool& showRing,
        bool& showCylinder,
        bool& showSphere,
        bool& showParticle,
        Math::Vector4& cylinderColor,
        float& cylinderAlphaReference,
        float& cylinderUVScrollSpeed,
        Math::Vector3& pointLightPosition,
        float& pointLightIntensity,
        Math::Vector3& spotLightPosition,
        Math::Vector3& spotLightDirection,
        float& spotLightIntensity);

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srvHeap_;
};
