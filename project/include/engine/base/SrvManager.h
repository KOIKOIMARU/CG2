#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <array>
#include <cstdint>
#include <vector>

class DirectXCommon;

// シェーダー可視SRV/UAVディスクリプタヒープの割り当てを管理する。
// Allocateで得たインデックスは所有者が保持し、不要になった時点で一度だけFreeする。
class SrvManager {
public:
    // 1つのシェーダー可視ヒープで確保できる最大ディスクリプタ数。
    static constexpr uint32_t kMaxSRVCount = 512;

    // DirectXCommonを借用してディスクリプタヒープを生成する。
    void Initialize(DirectXCommon* dxCommon);

    // ===== 確保 =====
    uint32_t Allocate();
    bool CanAllocate(uint32_t count) const;
    void Free(uint32_t index);
    void Free(D3D12_CPU_DESCRIPTOR_HANDLE handle);

    // ===== ハンドル取得 =====
    D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(uint32_t index);
    D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(uint32_t index);

    // ===== SRV生成 =====
    void CreateSRVforTexture2D(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT mipLevels
    );

    void CreateSRVforTextureCube(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        DXGI_FORMAT format,
        UINT mipLevels
    );

    void CreateSRVforStructuredBuffer(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        UINT numElements,
        UINT structureByteStride
    );

    void CreateUAVforStructuredBuffer(
        uint32_t srvIndex,
        ID3D12Resource* resource,
        UINT numElements,
        UINT structureByteStride
    );

    // ===== 描画前処理 =====
    void PreDraw();

    // ===== 描画時SRVセット =====
    void SetGraphicsRootDescriptorTable(
        UINT rootParameterIndex,
        uint32_t srvIndex
    );

    void SetComputeRootDescriptorTable(
        UINT rootParameterIndex,
        uint32_t srvIndex
    );

    // ===== DescriptorHeap取得 =====
    ID3D12DescriptorHeap* GetDescriptorHeap() const {
        return descriptorHeap_.Get();
    }

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> GetDescriptorHeapComPtr() const { return descriptorHeap_; }


private:
    DirectXCommon* directXCommon_ = nullptr; // デバイスとコマンドリストの借用先

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> descriptorHeap_; // SRV/UAVを格納するGPU可視ヒープ
    uint32_t descriptorSize_ = 0; // 1ディスクリプタ分のバイト間隔

    uint32_t useIndex_ = 0;               // まだ一度も使っていない次の番号
    std::vector<uint32_t> freeIndices_;   // Freeによって再利用可能になった番号
    std::array<bool, kMaxSRVCount> allocated_{}; // 二重解放と未確保番号の利用を検出する
};
