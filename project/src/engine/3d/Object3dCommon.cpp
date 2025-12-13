#include "engine/3d/Object3dCommon.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/Logger.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

void Object3dCommon::Initialize(DirectXCommon* dxCommon) {
    assert(dxCommon);
    dxCommon_ = dxCommon;

    // 資料の順番：PSO作る前に RootSig
    CreateRootSignature();
    CreateGraphicsPipelineState();
}

void Object3dCommon::CreateRootSignature() {
    auto* device = dxCommon_->GetDevice();
    HRESULT hr = S_OK;

    // ↓ ここに main.cpp の RootSignature 作成処理を “ほぼそのまま” 移す
    // - D3D12_ROOT_SIGNATURE_DESC
    // - rootParameters
    // - sampler
    // - Serialize
    // - device->CreateRootSignature(...)

    // 例：最後がこうなる
    // hr = device->CreateRootSignature(..., IID_PPV_ARGS(&rootSignature_));
    // assert(SUCCEEDED(hr));
}

void Object3dCommon::CreateGraphicsPipelineState() {
    auto* device = dxCommon_->GetDevice();
    HRESULT hr = S_OK;

    // ↓ ここに main.cpp の PSO 作成処理を “ほぼそのまま” 移す
    // - InputLayout
    // - Blend/Rasterizer/DepthStencil
    // - Shader compile（dxCommon_->CompileShader を使ってるならそのまま）
    // - D3D12_GRAPHICS_PIPELINE_STATE_DESC
    // - device->CreateGraphicsPipelineState(...)

    // hr = device->CreateGraphicsPipelineState(..., IID_PPV_ARGS(&pipelineState_));
    // assert(SUCCEEDED(hr));
}
