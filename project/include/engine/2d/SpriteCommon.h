#pragma once
#include <d3d12.h>
#include <wrl.h>

class DirectXCommon;
class TextureManager;
class SrvManager;

// すべての2Dスプライトで共有する描画パイプラインを管理する。
class SpriteCommon {
public:
    // 描画デバイスとテクスチャ管理機構を借用して初期化する。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    // Spriteが描画コマンドを設定するための共有オブジェクトを返す。
    ID3D12RootSignature* GetRootSignature() const { return rootSignature_.Get(); }
    ID3D12PipelineState* GetPipelineState() const { return pipelineState_.Get(); }

    DirectXCommon* GetDxCommon() const { return dxCommon_; }
    SrvManager* GetSrvManager() const { return srvManager_; }

    void CommonDrawSetting();

private:
    // スプライト描画用のルートシグネチャを生成する。
    void CreateRootSignature();

    // スプライト描画用のパイプラインを生成する。
    void CreateGraphicsPipelineState();

private:
    DirectXCommon* dxCommon_ = nullptr; // デバイスとコマンドリストの借用先

    Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature_; // スプライト用ルート引数定義
    Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState_; // スプライト用PSO
    SrvManager* srvManager_ = nullptr;                          // テクスチャSRVを設定する借用先
};
