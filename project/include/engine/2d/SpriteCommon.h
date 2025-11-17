#pragma once
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;   // 前方宣言

class SpriteCommon {
public:
    // 共通部の初期化（スライドの Initialize 相当）
    void Initialize(DirectXCommon* dxCommon);

    // 3D側や Sprite クラスから参照できるように getter だけ用意
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

    DirectXCommon* GetDxCommon() const { return dxCommon_; }

    void CommonDrawSetting();

private:
    // スライドの「ルートシグネチャの作成」
    void CreateRootSignature();

    // スライドの「グラフィックスパイプラインの生成」
    void CreateGraphicsPipelineState();

private:
    // スライドの「DirectXCommon のポインタ」
    DirectXCommon* dxCommon_ = nullptr;

    // ルートシグネチャ
    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_;

    // グラフィックスパイプラインステート
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_;
};
