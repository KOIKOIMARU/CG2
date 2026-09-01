#pragma once

#include <unordered_map>
#include <string>
#include <memory>
#include <cstdint>



#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

class DirectXCommon;
class SrvManager;

// GPUパーティクルのグループ生成、発生、更新、描画を一括管理する。
class ParticleManager
{
public:
    struct EmitSettings
    {
        float radius = 1.0f;                               // 発生中心からの最大距離
        Math::Vector3 direction{ 0.0f, 1.0f, 0.0f };       // 粒が飛ぶ基準方向
        float spread = 1.0f;                               // 基準方向からのランダムな広がり
        Math::Vector4 colorMin{ 1.0f, 1.0f, 1.0f, 1.0f };  // 生成色の乱数下限
        Math::Vector4 colorMax{ 1.0f, 1.0f, 1.0f, 1.0f };  // 生成色の乱数上限
        Math::Vector2 scaleMin{ 0.12f, 0.12f };             // 生成サイズの乱数下限
        Math::Vector2 scaleMax{ 0.30f, 0.30f };             // 生成サイズの乱数上限
        float lifeTimeMin = 1.0f;                           // 寿命の乱数下限（秒）
        float lifeTimeMax = 2.0f;                           // 寿命の乱数上限（秒）
        float speedMin = 0.4f;                              // 初速の乱数下限
        float speedMax = 1.6f;                              // 初速の乱数上限
        float endScale = 1.0f;                              // 消滅直前の初期サイズに対する倍率
    };

    // ===== シングルトン =====
    static ParticleManager* GetInstance();

    // コピー禁止
    ParticleManager(const ParticleManager&) = delete;
    ParticleManager& operator=(const ParticleManager&) = delete;

    // ===== 初期化 / 終了 =====
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Finalize();

    // ===== 更新 / 描画 =====
    void Update(const Math::Matrix4x4& viewMatrix,
        const Math::Matrix4x4& projectionMatrix);

    void Draw();

    // ===== パーティクル発生 =====
    bool CreateParticleGroup(const std::string& name,
        const std::string& textureFilePath,
        const EmitSettings& settings = {});
    bool RemoveParticleGroup(const std::string& name);

    void Emit(const std::string& name,
        const Math::Vector3& position,
        uint32_t count);
    void Emit(const std::string& name,
        const Math::Vector3& position,
        const Math::Vector3& direction,
        uint32_t count);

private:
    ParticleManager() = default;
    ~ParticleManager() = default;

    // HLSLのStructured Bufferと同じ並びを保つGPUパーティクル状態。
    struct ParticleCS
    {
        Math::Vector3 translate;     // 現在のワールド位置
        Math::Vector3 scale;         // 現在の描画サイズ
        float lifeTime;              // 粒が生存する秒数
        Math::Vector3 velocity;      // 1秒あたりの移動量
        float currentTime;           // 生成後の経過秒数
        Math::Vector4 color;         // 描画するRGBA色
        Math::Vector3 initialScale;  // 補間開始時のサイズ
        float endScale;              // 消滅時のサイズ倍率
    };

    struct PerView
    {
        Math::Matrix4x4 viewProjection; // ワールド座標からクリップ座標への変換
        Math::Matrix4x4 billboardMatrix; // 粒をカメラ正面へ向ける回転行列
    };

    struct EmitterSphere
    {
        Math::Vector3 translate; // ワールド空間上の発生中心
        float radius;            // 球状発生範囲の半径
        Math::Vector3 direction; // 飛翔の基準方向
        float spread;            // 方向のばらつき
        Math::Vector4 colorMin;  // 生成色の乱数下限
        Math::Vector4 colorMax;  // 生成色の乱数上限
        Math::Vector2 scaleMin;  // 生成サイズの乱数下限
        Math::Vector2 scaleMax;  // 生成サイズの乱数上限
        float lifeTimeMin;       // 寿命の乱数下限
        float lifeTimeMax;       // 寿命の乱数上限
        float speedMin;          // 初速の乱数下限
        float speedMax;          // 初速の乱数上限
        float endScale;          // 消滅時のサイズ倍率
        uint32_t count;          // 今回生成する粒数
        uint32_t emit;           // 今フレームに生成要求がある場合は1
        float padding;           // 16バイト境界にそろえる余白
    };

    struct PerFrame
    {
        float time;             // アプリ開始後の累積秒数
        float deltaTime;        // 今フレームの経過秒数
        Math::Vector2 padding;  // 16バイト境界にそろえる余白
    };

    // ============================
    // パーティクルグループ
    // ============================
    struct ParticleGroup
    {
        std::string textureFilePath;       // 粒の描画に使うテクスチャ
        uint32_t textureSrvIndex = 0;      // テクスチャSRVのヒープ番号

        // --- インスタンシング ---
        uint32_t particleSrvIndex = UINT32_MAX;    // 描画から粒を読むSRV番号
        uint32_t particleUavIndex = UINT32_MAX;    // Compute Shaderが粒へ書くUAV番号
        uint32_t freeCounterUavIndex = UINT32_MAX; // 空き数カウンターUAV番号
        uint32_t freeListUavIndex = UINT32_MAX;    // 空きスロット一覧UAV番号
        uint32_t instanceCount = 0;                // 描画へ渡す固定インスタンス数
        bool needsInitialize = true;               // GPU上の空き一覧を初期化する必要があるか
        bool needsEmit = false;                    // 次の更新で発生処理を行うか

        Microsoft::WRL::ComPtr<ID3D12Resource> particleResource;    // 全粒のGPU状態バッファ
        Microsoft::WRL::ComPtr<ID3D12Resource> freeCounterResource;// 未使用スロット数バッファ
        Microsoft::WRL::ComPtr<ID3D12Resource> freeListResource;   // 未使用スロット番号バッファ
        Microsoft::WRL::ComPtr<ID3D12Resource> emitterResource;    // 今回の発生条件バッファ
        EmitterSphere* emitterData = nullptr;                       // 発生条件バッファのCPU書込み先
        D3D12_RESOURCE_STATES particleResourceState = D3D12_RESOURCE_STATE_COMMON; // 粒バッファの現在状態
        D3D12_RESOURCE_STATES freeCounterResourceState = D3D12_RESOURCE_STATE_COMMON; // カウンターの現在状態
        D3D12_RESOURCE_STATES freeListResourceState = D3D12_RESOURCE_STATE_COMMON; // 空き一覧の現在状態
    };

private:
    DirectXCommon* dxCommon_ = nullptr; // デバイスとコマンドリストの借用先
    SrvManager* srvManager_ = nullptr;  // SRV/UAVを確保する借用先

    std::unordered_map<std::string, ParticleGroup> particleGroups_; // 名前ごとの所有グループ

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;           // 描画用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;           // ビルボード描画用PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> initializeRootSignature_; // 空き一覧初期化用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> initializePipelineState_; // 空き一覧初期化用Compute PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> emitRootSignature_;       // 粒生成用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> emitPipelineState_;       // 粒生成用Compute PSO
    Microsoft::WRL::ComPtr<ID3D12RootSignature> updateRootSignature_;     // 粒更新用ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12PipelineState> updatePipelineState_;     // 粒更新用Compute PSO

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_; // 1枚の粒を表す板ポリゴン
    D3D12_VERTEX_BUFFER_VIEW vbView_{};                      // 板ポリゴンの頂点ビュー
    Microsoft::WRL::ComPtr<ID3D12Resource> perViewResource_;// カメラごとの定数バッファ
    PerView* perViewData_ = nullptr;                         // 上記バッファのCPU書込み先
    Microsoft::WRL::ComPtr<ID3D12Resource> perFrameResource_;// 時刻情報の定数バッファ
    PerFrame* perFrameData_ = nullptr;                       // 上記バッファのCPU書込み先
    float totalTime_ = 0.0f;                                // 初期化後の累積秒数

    static constexpr uint32_t kMaxInstanceCount_ = 1024; // 1グループが保持できる最大粒数

    void CreateInitializePipeline();
    void CreateEmitPipeline();
    void CreateUpdatePipeline();
    void DispatchInitialize(ParticleGroup& group);
    void DispatchEmit(ParticleGroup& group);
    void DispatchUpdate(ParticleGroup& group);
    bool IsGpuPipelineReady() const;
    void ReleaseParticleGroupDescriptors(ParticleGroup& group);

};
