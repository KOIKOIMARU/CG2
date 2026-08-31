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

// 3Dオブジェクトの色合成方法。Countは配列確保用の終端値。
enum class BlendMode : uint32_t {
    None,
    Normal,
    Add,
    Subtract,
    Multiply,
    Screen,
    Count,
};

// 深度バッファの参照・書き込み方法。Overlayは前景演出向け。
enum class DepthDrawMode : uint32_t {
    Normal,
    ReadOnly,
    Overlay,
    Count,
};

// Object3d間で共有するルートシグネチャ、PSO、影描画資源を管理する。
// 各Object3dを初期化する前に本クラスを初期化し、描画直前にCommonDrawSettingを呼ぶ。
class Object3dCommon {
public:
    // 引数は借用。呼び出し側が本クラスより長く生存させる。
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

    // 通常描画用PSOとルートシグネチャをコマンドリストへ設定する。
    void CommonDrawSetting();
    // 影マップ生成用PSOとルートシグネチャを設定する。
    void CommonShadowDrawSetting();
    // 影パスを開始する。trueのときだけ影描画を行い、最後にEndShadowPassを呼ぶ。
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

    // カメラは借用。描画中に破棄しないこと。
    void SetDefaultCamera(Camera* camera) {
        defaultCamera_ = camera;
    }

    Camera* GetDefaultCamera() const {
        return defaultCamera_;
    }

    // Object3dへ既定値として渡す環境マップのパスを設定する。
    void SetEnvironmentTexturePath(const std::string& texturePath) {
        environmentTexturePath_ = texturePath;
    }

    const std::string& GetEnvironmentTexturePath() const {
        return environmentTexturePath_;
    }

private:
    // 影マップは描画先とシェーダー入力で状態を切り替えるため、遷移をこのクラスへ集約する。
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
