#pragma once
#include <array>
#include <cstdint>
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

class Object3dCommon {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager
    );

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const {
        return pipelineStates_[static_cast<size_t>(blendMode_)].Get();
    }

    DirectXCommon* GetDxCommon() const { return dxCommon_; } // 資料の getter

    void CommonDrawSetting();
    void SetBlendMode(BlendMode blendMode) { blendMode_ = blendMode; }
    BlendMode GetBlendMode() const { return blendMode_; }

    void SetDefaultCamera(Camera* camera) {
        defaultCamera_ = camera;
    }

    Camera* GetDefaultCamera() const {
        return defaultCamera_;
    }

private:
    void CreateRootSignature();
    void CreateGraphicsPipelineState();

private:
    DirectXCommon* dxCommon_ = nullptr; // 借り物。絶対 delete しない
    SrvManager* srvManager_ = nullptr;


    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    std::array<
        Microsoft::WRL::ComPtr<ID3D12PipelineState>,
        static_cast<size_t>(BlendMode::Count)> pipelineStates_;
    BlendMode blendMode_ = BlendMode::Normal;

    Camera* defaultCamera_ = nullptr;
};
