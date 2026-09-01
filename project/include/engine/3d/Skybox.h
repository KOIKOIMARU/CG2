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
        Math::Vector4 position; // キューブを構成するモデル空間の頂点位置
    };

    struct ViewProjectionData {
        Math::Matrix4x4 viewProjection; // 平行移動を除いたビュー・投影合成行列
    };

    // Skybox固有の描画資源を初期化時にまとめて生成する。
    void CreateRootSignature();
    void CreateGraphicsPipelineState();
    void CreateVertexBuffer();
    void CreateViewProjectionResource();

private:
    DirectXCommon* dxCommon_ = nullptr; // デバイスとコマンドリストの借用先
    SrvManager* srvManager_ = nullptr;  // キューブマップSRVを参照する借用先
    std::string texturePath_;           // 背景に描画するキューブマップの登録名

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // Skybox用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_; // Skybox用PSO

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_; // キューブの頂点バッファ
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};            // 頂点バッファの描画ビュー
    uint32_t vertexCount_ = 0;                               // 描画する頂点数

    Microsoft::WRL::ComPtr<ID3D12Resource> viewProjectionResource_; // カメラ行列の定数バッファ
    ViewProjectionData* viewProjectionData_ = nullptr;               // 上記バッファのCPU書込み先
};
