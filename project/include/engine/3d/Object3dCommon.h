#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

class DirectXCommon;
class Camera;
class SrvManager;

enum class BlendMode : uint32_t {
    None,
    Normal,
    Add,
    Subtract,
    Multiply,
    Screen,
    Count,
};

enum class DepthDrawMode : uint32_t {
    Normal,
    ReadOnly,
    Overlay,
    Count,
};

class Object3dCommon {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager
    );

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const {
        return pipelineStates_[static_cast<size_t>(depthDrawMode_)]
            [static_cast<size_t>(blendMode_)].Get();
    }

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    SrvManager* GetSrvManager() const { return srvManager_; }

    void CommonDrawSetting();
    void CommonShadowDrawSetting();
    bool BeginShadowPass(const Math::Vector3& focusCenter);
    void EndShadowPass();
    const Math::Matrix4x4& GetShadowLightViewProjection() const {
        return shadowLightViewProjection_;
    }
    uint32_t GetShadowMapSrvIndex() const { return shadowMapSrvIndex_; }
    bool IsShadowMapReady() const { return shadowMapReady_; }
    float GetShadowStrength() const { return shadowStrength_; }
    float GetShadowBias() const { return shadowBias_; }
    float GetShadowNormalBias() const { return shadowNormalBias_; }
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }
    BlendMode GetBlendMode() const { return blendMode_; }
    void SetDepthDrawMode(DepthDrawMode depthDrawMode) {
        depthDrawMode_ = depthDrawMode;
    }
    DepthDrawMode GetDepthDrawMode() const { return depthDrawMode_; }

    void SetDefaultCamera(Camera* camera) {
        defaultCamera_ = camera;
    }

    Camera* GetDefaultCamera() const {
        return defaultCamera_;
    }

    void SetEnvironmentTexturePath(const std::string& texturePath) {
        environmentTexturePath_ = texturePath;
    }

    const std::string& GetEnvironmentTexturePath() const {
        return environmentTexturePath_;
    }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateShadowMap();
    void TransitionShadowMap(D3D12_RESOURCE_STATES nextState);
    void RestoreMainRenderTarget();

private:
    static constexpr uint32_t kShadowMapSize = 1024;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::array<
        std::array<
            Microsoft::WRL::ComPtr<ID3D12PipelineState>,
            static_cast<size_t>(BlendMode::Count)>,
        static_cast<size_t>(DepthDrawMode::Count)> pipelineStates_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> shadowPipelineState_;
    BlendMode blendMode_ = BlendMode::Normal;
    DepthDrawMode depthDrawMode_ = DepthDrawMode::Normal;

    Camera* defaultCamera_ = nullptr;
    std::string environmentTexturePath_;

    Microsoft::WRL::ComPtr<ID3D12Resource> shadowMapResource_;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> shadowMapDsvHeap_;
    D3D12_RESOURCE_STATES shadowMapState_ =
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
    D3D12_VIEWPORT shadowViewport_{};
    D3D12_RECT shadowScissorRect_{};
    Math::Matrix4x4 shadowLightViewProjection_ = Math::MakeIdentity4x4();
    Math::Vector3 shadowLightDirection_{ 0.30f, -0.86f, 0.41f };
    uint32_t shadowMapSrvIndex_ = UINT32_MAX;
    float shadowStrength_ = 0.34f;
    float shadowBias_ = 0.0018f;
    float shadowNormalBias_ = 0.0035f;
    bool shadowMapReady_ = false;
};
