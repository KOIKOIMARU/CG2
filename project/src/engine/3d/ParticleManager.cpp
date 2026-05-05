#include "engine/3d/ParticleManager.h"

#include <array>
#include <cassert>
#include <cstring>


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

    // ===== Viewごとの情報 =====
    perViewResource_ = dxCommon_->CreateBufferResource(256);
    perViewResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&perViewData_)
    );
    perViewData_->viewProjection = MakeIdentity4x4();
    perViewData_->billboardMatrix = MakeIdentity4x4();

    // ===== フレームごとの情報 =====
    perFrameResource_ = dxCommon_->CreateBufferResource(256);
    perFrameResource_->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&perFrameData_)
    );
    perFrameData_->time = 0.0f;
    perFrameData_->deltaTime = 0.0f;

    // ===== RootSignature =====
    {
        CD3DX12_DESCRIPTOR_RANGE rangeTex{};
        rangeTex.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

        CD3DX12_DESCRIPTOR_RANGE rangeParticle{};
        rangeParticle.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

        CD3DX12_ROOT_PARAMETER rootParams[3]{};
        rootParams[0].InitAsDescriptorTable(1, &rangeTex, D3D12_SHADER_VISIBILITY_PIXEL);
        rootParams[1].InitAsDescriptorTable(1, &rangeParticle, D3D12_SHADER_VISIBILITY_VERTEX);
        rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_VERTEX);

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

    CreateInitializePipeline();
    CreateEmitPipeline();
}


void ParticleManager::CreateInitializePipeline()
{
    CD3DX12_DESCRIPTOR_RANGE rangeParticle{};
    rangeParticle.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

    CD3DX12_DESCRIPTOR_RANGE rangeCounter{};
    rangeCounter.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1); // u1

    CD3DX12_ROOT_PARAMETER rootParams[2]{};
    rootParams[0].InitAsDescriptorTable(1, &rangeParticle, D3D12_SHADER_VISIBILITY_ALL);
    rootParams[1].InitAsDescriptorTable(1, &rangeCounter, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> sigBlob;
    ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rsDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &sigBlob,
        &errBlob
    );
    if (FAILED(hr)) {
        if (errBlob) { Logger::Log(reinterpret_cast<const char*>(errBlob->GetBufferPointer())); }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&initializeRootSignature_)
    );
    assert(SUCCEEDED(hr));

    auto cs = dxCommon_->CompileShader(L"shaders/InitializeParticle.CS.hlsl", L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = initializeRootSignature_.Get();
    psoDesc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

    hr = dxCommon_->GetDevice()->CreateComputePipelineState(
        &psoDesc,
        IID_PPV_ARGS(&initializePipelineState_)
    );
    assert(SUCCEEDED(hr));
}

void ParticleManager::CreateEmitPipeline()
{
    CD3DX12_DESCRIPTOR_RANGE rangeParticle{};
    rangeParticle.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0); // u0

    CD3DX12_DESCRIPTOR_RANGE rangeCounter{};
    rangeCounter.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 1); // u1

    CD3DX12_ROOT_PARAMETER rootParams[4]{};
    rootParams[0].InitAsDescriptorTable(1, &rangeParticle, D3D12_SHADER_VISIBILITY_ALL);
    rootParams[1].InitAsDescriptorTable(1, &rangeCounter, D3D12_SHADER_VISIBILITY_ALL);
    rootParams[2].InitAsConstantBufferView(0, 0, D3D12_SHADER_VISIBILITY_ALL);
    rootParams[3].InitAsConstantBufferView(1, 0, D3D12_SHADER_VISIBILITY_ALL);

    CD3DX12_ROOT_SIGNATURE_DESC rsDesc{};
    rsDesc.Init(_countof(rootParams), rootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);

    ComPtr<ID3DBlob> sigBlob;
    ComPtr<ID3DBlob> errBlob;
    HRESULT hr = D3D12SerializeRootSignature(
        &rsDesc,
        D3D_ROOT_SIGNATURE_VERSION_1,
        &sigBlob,
        &errBlob
    );
    if (FAILED(hr)) {
        if (errBlob) { Logger::Log(reinterpret_cast<const char*>(errBlob->GetBufferPointer())); }
        assert(false);
    }

    hr = dxCommon_->GetDevice()->CreateRootSignature(
        0,
        sigBlob->GetBufferPointer(),
        sigBlob->GetBufferSize(),
        IID_PPV_ARGS(&emitRootSignature_)
    );
    assert(SUCCEEDED(hr));

    auto cs = dxCommon_->CompileShader(L"shaders/EmitParticle.CS.hlsl", L"cs_6_0");

    D3D12_COMPUTE_PIPELINE_STATE_DESC psoDesc{};
    psoDesc.pRootSignature = emitRootSignature_.Get();
    psoDesc.CS = { cs->GetBufferPointer(), cs->GetBufferSize() };

    hr = dxCommon_->GetDevice()->CreateComputePipelineState(
        &psoDesc,
        IID_PPV_ARGS(&emitPipelineState_)
    );
    assert(SUCCEEDED(hr));
}

void ParticleManager::DispatchInitialize(ParticleGroup& group)
{
    auto* cl = dxCommon_->GetCommandList();

    if (group.particleResourceState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = group.particleResource.Get();
        barrier.Transition.StateBefore = group.particleResourceState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cl->ResourceBarrier(1, &barrier);
        group.particleResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    if (group.freeCounterResourceState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = group.freeCounterResource.Get();
        barrier.Transition.StateBefore = group.freeCounterResourceState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cl->ResourceBarrier(1, &barrier);
        group.freeCounterResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    cl->SetComputeRootSignature(initializeRootSignature_.Get());
    cl->SetPipelineState(initializePipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(0, group.particleUavIndex);
    srvManager_->SetComputeRootDescriptorTable(1, group.freeCounterUavIndex);
    cl->Dispatch(1, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarrier{};
    uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarrier.UAV.pResource = group.particleResource.Get();
    cl->ResourceBarrier(1, &uavBarrier);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = group.particleResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &barrier);

    group.particleResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;
    group.needsInitialize = false;
}

void ParticleManager::DispatchEmit(ParticleGroup& group)
{
    if (!group.needsEmit || !group.emitterData || group.emitterData->emit == 0) {
        return;
    }

    auto* cl = dxCommon_->GetCommandList();

    if (group.particleResourceState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = group.particleResource.Get();
        barrier.Transition.StateBefore = group.particleResourceState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cl->ResourceBarrier(1, &barrier);
        group.particleResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    if (group.freeCounterResourceState != D3D12_RESOURCE_STATE_UNORDERED_ACCESS) {
        D3D12_RESOURCE_BARRIER barrier{};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = group.freeCounterResource.Get();
        barrier.Transition.StateBefore = group.freeCounterResourceState;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        cl->ResourceBarrier(1, &barrier);
        group.freeCounterResourceState = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    }

    cl->SetComputeRootSignature(emitRootSignature_.Get());
    cl->SetPipelineState(emitPipelineState_.Get());
    srvManager_->SetComputeRootDescriptorTable(0, group.particleUavIndex);
    srvManager_->SetComputeRootDescriptorTable(1, group.freeCounterUavIndex);
    cl->SetComputeRootConstantBufferView(2, group.emitterResource->GetGPUVirtualAddress());
    cl->SetComputeRootConstantBufferView(3, perFrameResource_->GetGPUVirtualAddress());
    cl->Dispatch(1, 1, 1);

    D3D12_RESOURCE_BARRIER uavBarriers[2]{};
    uavBarriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[0].UAV.pResource = group.particleResource.Get();
    uavBarriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    uavBarriers[1].UAV.pResource = group.freeCounterResource.Get();
    cl->ResourceBarrier(_countof(uavBarriers), uavBarriers);

    D3D12_RESOURCE_BARRIER barrier{};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = group.particleResource.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_GENERIC_READ;
    barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    cl->ResourceBarrier(1, &barrier);
    group.particleResourceState = D3D12_RESOURCE_STATE_GENERIC_READ;

    group.needsEmit = false;
}

void ParticleManager::Update(
    const Matrix4x4& viewMatrix,
    const Matrix4x4& projectionMatrix
) {
    if (!perViewData_) {
        return;
    }

    const float deltaTime = dxCommon_->GetDeltaTime();
    totalTime_ += deltaTime;

    // ViewProjectionとBillboardはViewごとの値なので、まとめてConstantBufferへ書く
    perViewData_->viewProjection = Multiply(viewMatrix, projectionMatrix);
    perViewData_->billboardMatrix = Inverse(viewMatrix);
    perViewData_->billboardMatrix.m[3][0] = 0.0f;
    perViewData_->billboardMatrix.m[3][1] = 0.0f;
    perViewData_->billboardMatrix.m[3][2] = 0.0f;

    if (perFrameData_) {
        perFrameData_->time = totalTime_;
        perFrameData_->deltaTime = deltaTime;
    }
}

void ParticleManager::Draw()
{
    auto* cl = dxCommon_->GetCommandList();



    cl->SetGraphicsRootSignature(rootSignature_.Get());
    cl->SetPipelineState(pipelineState_.Get());
    cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    cl->IASetVertexBuffers(0, 1, &vbView_);
    cl->SetGraphicsRootConstantBufferView(2, perViewResource_->GetGPUVirtualAddress());

    for (auto& [name, group] : particleGroups_) {
        // GPU Particleの中身はComputeShaderで一度だけ初期化する
        if (group.needsInitialize) {
            DispatchInitialize(group);

            cl->SetGraphicsRootSignature(rootSignature_.Get());
            cl->SetPipelineState(pipelineState_.Get());
            cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
            cl->IASetVertexBuffers(0, 1, &vbView_);
            cl->SetGraphicsRootConstantBufferView(2, perViewResource_->GetGPUVirtualAddress());
        }

        // CPU側Emitterから射出許可が出ていれば、描画前にGPUでParticleを追加する
        DispatchEmit(group);
        cl->SetGraphicsRootSignature(rootSignature_.Get());
        cl->SetPipelineState(pipelineState_.Get());
        cl->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        cl->IASetVertexBuffers(0, 1, &vbView_);
        cl->SetGraphicsRootConstantBufferView(2, perViewResource_->GetGPUVirtualAddress());

        if (group.instanceCount == 0) continue;

        srvManager_->SetGraphicsRootDescriptorTable(0, group.textureSrvIndex);
        srvManager_->SetGraphicsRootDescriptorTable(1, group.particleSrvIndex);

        cl->DrawInstanced(6, group.instanceCount, 0, 0);
    }
}


// ======================================
// 終了処理
// ======================================
void ParticleManager::Finalize()
{
    // Particle用SRV/UAVを解放
    if (srvManager_) {
        for (auto& [name, group] : particleGroups_) {
            if (group.particleSrvIndex != UINT32_MAX) {
                srvManager_->Free(group.particleSrvIndex);
            }
            if (group.particleUavIndex != UINT32_MAX) {
                srvManager_->Free(group.particleUavIndex);
            }
            if (group.freeCounterUavIndex != UINT32_MAX) {
                srvManager_->Free(group.freeCounterUavIndex);
            }
        }
    }

    particleGroups_.clear();
    perViewData_ = nullptr;
    perViewResource_.Reset();
    perFrameData_ = nullptr;
    perFrameResource_.Reset();
    totalTime_ = 0.0f;
    vertexResource_.Reset();
    pipelineState_.Reset();
    rootSignature_.Reset();
    initializePipelineState_.Reset();
    initializeRootSignature_.Reset();
    emitPipelineState_.Reset();
    emitRootSignature_.Reset();

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

    // ④ Particle用 StructuredBuffer 作成
    group.particleResource =
        dxCommon_->CreateBufferResource(
            sizeof(ParticleCS) * kMaxInstanceCount_,
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );
    group.particleResourceState = D3D12_RESOURCE_STATE_COMMON;
    group.instanceCount = kMaxInstanceCount_;
    group.needsInitialize = true;
    group.needsEmit = false;

    // ⑤ 空きParticle番号を管理するCounterを作成
    group.freeCounterResource =
        dxCommon_->CreateBufferResource(
            sizeof(int32_t),
            D3D12_HEAP_TYPE_DEFAULT,
            D3D12_RESOURCE_STATE_COMMON,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
        );
    group.freeCounterResourceState = D3D12_RESOURCE_STATE_COMMON;

    // ⑥ Emitter情報をConstantBufferとして作成
    group.emitterResource = dxCommon_->CreateBufferResource(256);
    group.emitterResource->Map(
        0,
        nullptr,
        reinterpret_cast<void**>(&group.emitterData)
    );
    group.emitterData->translate = { 0.0f, 0.0f, 0.0f };
    group.emitterData->radius = 1.0f;
    group.emitterData->count = 10;
    group.emitterData->frequency = 0.5f;
    group.emitterData->frequencyTime = 0.0f;
    group.emitterData->emit = 0;

    // ⑦ SRV/UAV 確保
    group.particleSrvIndex = srvManager_->Allocate();
    group.particleUavIndex = srvManager_->Allocate();
    group.freeCounterUavIndex = srvManager_->Allocate();

    // ⑧ StructuredBuffer 用 SRV/UAV 作成
    srvManager_->CreateSRVforStructuredBuffer(
        group.particleSrvIndex,
        group.particleResource.Get(),
        kMaxInstanceCount_,
        sizeof(ParticleCS)
    );
    srvManager_->CreateUAVforStructuredBuffer(
        group.particleUavIndex,
        group.particleResource.Get(),
        kMaxInstanceCount_,
        sizeof(ParticleCS)
    );
    srvManager_->CreateUAVforStructuredBuffer(
        group.freeCounterUavIndex,
        group.freeCounterResource.Get(),
        1,
        sizeof(int32_t)
    );

    // ⑨ コンテナに登録
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

    // ② CPU側で判断した射出情報をGPUへ渡す
    ParticleGroup& group = it->second;
    if (group.emitterData) {
        group.emitterData->translate = position;
        group.emitterData->radius = 1.0f;
        group.emitterData->count = count;
        group.emitterData->emit = 1;
        group.needsEmit = true;
    }
}

float ParticleManager::RandomFloat(float min, float max)
{
    std::uniform_real_distribution<float> dist(min, max);
    return dist(randomEngine_);
}
