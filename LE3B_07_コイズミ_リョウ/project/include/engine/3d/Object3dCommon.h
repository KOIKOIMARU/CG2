#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <wrl.h>
#include <d3d12.h>

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

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::array<
        std::array<
            Microsoft::WRL::ComPtr<ID3D12PipelineState>,
            static_cast<size_t>(BlendMode::Count)>,
        static_cast<size_t>(DepthDrawMode::Count)> pipelineStates_;
    BlendMode blendMode_ = BlendMode::Normal;
    DepthDrawMode depthDrawMode_ = DepthDrawMode::Normal;

    Camera* defaultCamera_ = nullptr;
    std::string environmentTexturePath_;
};
