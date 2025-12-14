#pragma once

#include <unordered_map>
#include <list>
#include <string>
#include <memory>
#include <random>



#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"
#include "engine/3d/Particle.h"

class DirectXCommon;
class SrvManager;

/// <summary>
/// パーティクル管理クラス（Singleton）
/// </summary>
class ParticleManager
{
public:
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
    void CreateParticleGroup(const std::string& name,
        const std::string& textureFilePath);

    void Emit(const std::string& name,
        const Math::Vector3& position,
        uint32_t count);

    float RandomFloat(float min, float max);

private:
    ParticleManager() = default;
    ~ParticleManager() = default;

    // ============================
    // インスタンシング用構造体
    // ============================
    struct ParticleInstanceData
    {
        Math::Matrix4x4 WVP;
    };

    // ============================
    // パーティクルグループ
    // ============================
    struct ParticleGroup
    {
        // --- マテリアル情報 ---
        std::string textureFilePath;
        uint32_t textureSrvIndex = 0;

        // --- パーティクル本体 ---
        std::list<Particle> particles;

        // --- インスタンシング ---
        uint32_t instanceSrvIndex = 0;
        uint32_t instanceCount = 0;

        Microsoft::WRL::ComPtr<ID3D12Resource> instanceResource;
        ParticleInstanceData* instanceData = nullptr;
    };

private:
    // ===== 借り物 =====
    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    // ===== グループ管理 =====
    std::unordered_map<std::string, ParticleGroup> particleGroups_;

    std::mt19937 randomEngine_;

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vbView_{};

    static constexpr uint32_t kMaxInstanceCount_ = 1024;

};
