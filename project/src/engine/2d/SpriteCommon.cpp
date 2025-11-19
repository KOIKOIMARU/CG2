#include "engine/2d/SpriteCommon.h"
#include "engine/base/DirectXCommon.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

void SpriteCommon::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon);
    dxCommon_ = dxCommon;

    // 共通部の初期化
    CreateGraphicsPipelineState();
}

void SpriteCommon::CreateRootSignature()
{
    // ★ここには後で main.cpp に書いてある
    //   ルートシグネチャ生成コードを丸ごと移してくる
}

void SpriteCommon::CreateGraphicsPipelineState()
{
    // ★ここには後で main.cpp に書いてある
    //   グラフィックスパイプライン生成コードを丸ごと移してくる
    //   （最初に CreateRootSignature() を呼ぶ感じ）
    CreateRootSignature();
}

void SpriteCommon::CommonDrawSetting()
{
    ID3D12GraphicsCommandList* commandList = dxCommon_->GetCommandList();

    // RootSignature をセット
    commandList->SetGraphicsRootSignature(rootSignature_.Get());

    // PSO をセット（変数名の修正）
    commandList->SetPipelineState(pipelineState_.Get());

    // プリミティブトポロジ（スプライトは三角形）
    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}
