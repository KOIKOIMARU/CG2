#pragma once
#include <wrl.h>
#include <d3d12.h>

class DirectXCommon;
class Camera;
class SrvManager;

class Object3dCommon {
public:
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager
    );

    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

    DirectXCommon* GetDxCommon() const { return dxCommon_; } // 資料の getter

    void CommonDrawSetting();

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
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Camera* defaultCamera_ = nullptr;
};
