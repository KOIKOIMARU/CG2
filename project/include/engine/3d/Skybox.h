#pragma once
#include <cstdint>
#include <string>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

class Camera;
class DirectXCommon;
class SrvManager;

// キューブマップをカメラの周囲へ描画する背景オブジェクト。
// Initialize後、毎フレームUpdateでカメラ行列を反映してからDrawする。
class Skybox {
public:
    // DirectXCommonとSrvManagerは借用。texturePathは読み込み済みキューブマップを指定する。
    void Initialize(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        const std::string& texturePath);

    // 平行移動を除いたビュー行列を生成し、空がカメラ移動に追従するようにする。
    void Update(const Camera* camera);
    void Draw();

private:
    struct VertexData {
        Math::Vector4 position;
    };

    struct ViewProjectionData {
        Math::Matrix4x4 viewProjection;
    };

    // Skybox固有の描画資源を初期化時にまとめて生成する。
    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateVertexBuffer();
    void CreateViewProjectionResource();

private:
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    std::string texturePath_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    uint32_t vertexCount_ = 0;

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_;
    ViewProjectionData* viewProjectionData_ = nullptr;
};
