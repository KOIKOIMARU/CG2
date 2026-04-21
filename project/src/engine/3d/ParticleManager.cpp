#include "engine/3d/ParticleManager.h"

#include <cassert>
#include <array>


#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/3d/TextureManager.h"
#include "engine/base/Logger.h"
#include "engine/base/StringUtility.h"

#include <d3dx12.h>

using namespace Microsoft::WRL;
using namespace Math;


namespace
{
    struct ParticleVertex
    {
        Vector4 position;
        Vector2 texcoord;
    };

    // 板ポリ（2三角形 = 6頂点）※中心(0,0)・サイズ1想定。必要ならスケールはWVP側で。
    std::array<ParticleVertex, 6> MakeQuadVertices()
    {
        // 左下(-0.5,-0.5) ～ 右上(0.5,0.5)
        return {
            ParticleVertex{ Vector4{-0.5f, -0.5f, 0.0f, 1.0f}, Vector2{0.0f, 1.0f} },
            ParticleVertex{ Vector4{-0.5f,  0.5f, 0.0f, 1.0f}, Vector2{0.0f, 0.0f} },
            ParticleVertex{ Vector4{ 0.5f, -0.5f, 0.0f, 1.0f}, Vector2{1.0f, 1.0f} },

            ParticleVertex{ Vector4{ 0.5f, -0.5f, 0.0f, 1.0f}, Vector2{1.0f, 1.0f} },
            ParticleVertex{ Vector4{-0.5f,  0.5f, 0.0f, 1.0f}, Vector2{0.0f, 0.0f} },
            ParticleVertex{ Vector4{ 0.5f,  0.5f, 0.0f, 1.0f}, Vector2{1.0f, 0.0f} },
        };
    }
}

ParticleManager* ParticleManager::GetInstance()
{
    static ParticleManager instance;
    return &instance;
}

void ParticleManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    assert(dxCommon);
    assert(srvManager);

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;

    particleGroups_.clear();
    randomEngine_.seed(std::random_device{}());

    // ===== 頂点 =====
    const auto vertices = MakeQuadVertices();

    vertexResource_ = dxCommon_->CreateBufferResource(sizeof(vertices));
    {
        void* mapped = nullptr;
        vertexResource_->Map(0, nullptr, &mapped);
        memcpy(mapped, vertices.data(), sizeof(vertices));
        vertexResource_->Unmap(0, nullptr);
    }

    vbView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vbView_.SizeInBytes = UINT(sizeof(vertices));
    vbView_.StrideInBytes = UINT(sizeof(ParticleVertex));

    // ===== RootSignature =====
    {
        CD3DX12_DESCRIPTOR_RANGE rangeTex{};
        rangeTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

        CD3DX12_DESCRIPTOR_RANGE rangeInstance{};
        rangeInstance.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 1); // t1

        CD3DX12_ROOT_PARAMETER rootParams[2]{};
        rootParams[0].InitAsDescriptorTable(1, &rangeTex, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[1].InitAsDescriptorTable(1, &rangeInstance, D3D12_SHADER_VISIBILITY_VERTEX);

        CD3DX12_STATIC_SAMPLER_DESC samplerDesc(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

        CD3DX12_ROOT_SIGNATURE_DESC rsDesc{};
        rsDesc.Init(_countof(rootParams), rootParams, 1, &samplerDesc,
            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3DBlob> sigBlob;
        ComPtr<ID3DBlob> errBlob;
        HRESULT hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &sigBlob, &errBlob);
        if (FAILED(hr)) {
            if (errBlob) { Logger::Log(reinterpret_cast<const char*>(errBlob->GetBufferPointer())); }
            assert(false);
        }

        hr = dxCommon_->GetDevice()->CreateRootSignature(
            0, sigBlob->GetBufferPointer(), sigBlob->GetBufferSize(),
            IID_PPV_ARGS(&rootSignature_));
        assert(SUCCEEDED(hr));
    }

    // ===== PSO =====
    {
        auto vs = dxCommon_->CompileShader(L"shaders/Particle.VS.hlsl", L"vs_6_0");
        auto ps = dxCommon_->CompileShader(L"shaders/Particle.PS.hlsl", L"ps_6_0");

        D3D12_INPUT_ELEMENT_DESC inputLayout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
              D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
              D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };

        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc{};
        psoDesc.pRootSignature = rootSignature_.Get();
        psoDesc.VS = { vs->GetBufferPointer(), vs->GetBufferSize() };
        psoDesc.PS = { ps->GetBufferPointer(), ps->GetBufferSize() };

        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
        psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
        psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
        psoDesc.BlendState.RenderTarget[0].BlendOpAlpha =
            D3D12_BLEND_OP_ADD;
        psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask =
            D3D12_COLOR_WRITE_ENABLE_ALL;
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState.DepthWriteMask =
            D3D12_DEPTH_WRITE_MASK_ZERO;
        psoDesc.DepthStencilState.DepthFunc =
            D3D12_COMPARISON_FUNC_LESS_EQUAL;
        psoDesc.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

        psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // dxCommonと合わせる
        psoDesc.SampleDesc.Count = 1;
        psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

        HRESULT hr = dxCommon_->GetDevice()->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pipelineState_));
        assert(SUCCEEDED(hr));
    }
}


void ParticleManager::Update(
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projectionMatrix
) {
    const float deltaTime = dxCommon_->GetDeltaTime();

    // 全グループ処理
    for (auto& [name, group] : particleGroups_) {

        uint32_t instanceIndex = 0;

        // パーティクル更新
        for (auto it = group.particles.begin();
            it != group.particles.end();) {

            Particle& p = *it;

            // 寿命チェック
            if (!p.IsAlive()) {
                it = group.particles.erase(it);
                continue;
            }

            if (instanceIndex >= kMaxInstanceCount_) {
                break; // これ以上は instanceData に書けない
            }

            // 加速 → 速度
            p.velocity += p.acceleration * deltaTime;

            // 移動
            p.position += p.velocity * deltaTime;

            // 経過時間
            p.currentTime += deltaTime;

            // -------------------------
            // 行列計算
            // -------------------------

            // World（平行移動のみ）
            Matrix4x4 world = MakeAffineMatrix(
                p.scale,
                p.rotate,
                p.position
            );

            // WVP
            Matrix4x4 wvp =
                Multiply(
                    Multiply(world, viewMatrix),
                    projectionMatrix
                );

            // -------------------------
            // インスタンシングデータ書き込み
            // -------------------------
            group.instanceData[instanceIndex].WVP = wvp;
            instanceIndex++;

            ++it;
        }

        // 実際に描画するインスタンス数
        group.instanceCount = instanceIndex;
    }
}

void ParticleManager::Draw()
{
    auto* cl = dxCommon_->GetCommandList();



    cl->SetGraphicsRootSignature(rootSignature_.Get());
    cl->SetPipelineState(pipelineState_.Get());
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl->IASetVertexBuffers(0, 1, &vbView_);

    // ★ 必ず初期化
    if (!particleGroups_.empty()) {
        auto& g = particleGroups_.begin()->second;
        srvManager_->SetGraphicsRootDescriptorTable(0, g.textureSrvIndex);
        srvManager_->SetGraphicsRootDescriptorTable(1, g.instanceSrvIndex);
    }

    for (auto& [name, group] : particleGroups_) {
        if (group.instanceCount == 0) continue;

        srvManager_->SetGraphicsRootDescriptorTable(0, group.textureSrvIndex);
        srvManager_->SetGraphicsRootDescriptorTable(1, group.instanceSrvIndex);

        cl->DrawInstanced(6, group.instanceCount, 0, 0);
    }
}


// ======================================
// 終了処理
// ======================================
void ParticleManager::Finalize()
{
    // インスタンシングバッファを解放
    for (auto& [name, group] : particleGroups_) {
        group.instanceResource.Reset();
        group.instanceData = nullptr;
    }

    particleGroups_.clear();

    dxCommon_ = nullptr;
    srvManager_ = nullptr;
}

void ParticleManager::CreateParticleGroup(
    const std::string& name,
    const std::string& textureFilePath
) {
    // ① 既に存在していないかチェック
    assert(particleGroups_.find(name) == particleGroups_.end());

    // ② 空のグループを作成
    ParticleGroup group{};

    // ③ テクスチャ設定
    group.textureFilePath = textureFilePath;

    // テクスチャ読み込み（事前ロードでもOK）
    TextureManager::GetInstance()->LoadTexture(textureFilePath);
    group.textureSrvIndex =
        TextureManager::GetInstance()->GetSrvIndex(textureFilePath);

    // ④ インスタンシング用 StructuredBuffer 作成
    const uint32_t kMaxInstanceCount = 1024;

    group.instanceResource =
        dxCommon_->CreateBufferResource(
            sizeof(ParticleInstanceData) * kMaxInstanceCount
        );

    // CPUマップ
    group.instanceResource->Map(
        0, nullptr,
        reinterpret_cast<void**>(&group.instanceData)
    );


    // ⑤ SRV 確保
    group.instanceSrvIndex = srvManager_->Allocate();

    // ⑥ StructuredBuffer 用 SRV 作成
    srvManager_->CreateSRVforStructuredBuffer(
        group.instanceSrvIndex,
        group.instanceResource.Get(),
        kMaxInstanceCount,
        sizeof(ParticleInstanceData)
    );

    // ⑦ コンテナに登録
    particleGroups_.emplace(name, group);
}

void ParticleManager::Emit(
    const std::string& name,
    const Vector3& position,
    uint32_t count
) {
    // ① グループ存在チェック
    auto it = particleGroups_.find(name);
    assert(it != particleGroups_.end());

    ParticleGroup& group = it->second;

    // ② 指定数分パーティクル生成
    for (uint32_t i = 0; i < count; ++i) {
        Particle particle{};

        // 初期位置
        particle.position = position;

        // 仮の速度（あとでランダム化する）
        particle.scale = { 0.05f, RandomFloat(0.4f, 1.5f), 1.0f };
        particle.rotate = {
            0.0f,
            0.0f,
            RandomFloat(-3.14159265f, 3.14159265f)
        };
        particle.velocity = { 0.0f, 0.0f, 0.0f };

        // 加速度（場の影響：今は無し）
        particle.acceleration = { 0.0f, 0.0f, 0.0f };

        // 寿命（仮）
        particle.lifeTime = 0.12f;

        // 経過時間
        particle.currentTime = 0.0f;

        // ③ グループに登録
        group.particles.push_back(particle);
    }
}

float ParticleManager::RandomFloat(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(randomEngine_);
}
