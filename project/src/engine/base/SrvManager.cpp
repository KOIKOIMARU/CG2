#include "engine/base/SrvManager.h"
#include "engine/base/DirectXCommon.h"
#include <cassert>

using Microsoft::WRL::ComPtr;

const uint32_t SrvManager::kMaxSRVCount = 512;

void SrvManager::Initialize(DirectXCommon* dxCommon)
{
    assert(dxCommon);
    directXCommon_ = dxCommon;

    // SRVヒープ生成
    descriptorHeap_ =
        directXCommon_->CreateDescriptorHeap(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,
            kMaxSRVCount,
            true
        );

    // デスクリプタサイズ取得
    descriptorSize_ =
        directXCommon_->GetDevice()->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV
        );

    useIndex_ = 0;
    freeIndices_.clear();
}

uint32_t SrvManager::Allocate()
{
    if (!freeIndices_.empty()) {
        uint32_t index = freeIndices_.back();
        freeIndices_.pop_back();
        return index;
    }

    assert(useIndex_ < kMaxSRVCount);

    uint32_t index = useIndex_;
    useIndex_++;
    return index;
}

bool SrvManager::CanAllocate(uint32_t count) const
{
    if (useIndex_ > kMaxSRVCount) {
        return false;
    }

    const size_t availableCount =
        freeIndices_.size() + static_cast<size_t>(kMaxSRVCount - useIndex_);
    return static_cast<size_t>(count) <= availableCount;
}

void SrvManager::Free(uint32_t index)
{
    assert(index < useIndex_);
    freeIndices_.push_back(index);
}

void SrvManager::Free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    assert(descriptorHeap_);
    assert(descriptorSize_ != 0);

    D3D12_CPU_DESCRIPTOR_HANDLE start =
        descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    assert(handle.ptr >= start.ptr);

    SIZE_T offset = handle.ptr - start.ptr;
    assert(offset % descriptorSize_ == 0);

    uint32_t index = static_cast<uint32_t>(offset / descriptorSize_);
    Free(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE
SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
    assert(descriptorHeap_);

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize_) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE
SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
    assert(descriptorHeap_);

    D3D12_GPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap_->GetGPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<UINT64>(descriptorSize_) * index;
    return handle;
}

void SrvManager::CreateSRVforTexture2D(
    uint32_t srvIndex,
    ID3D12Resource* resource,
    DXGI_FORMAT format,
    UINT mipLevels)
{
    assert(resource);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = mipLevels;

    directXCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        GetCPUDescriptorHandle(srvIndex)
    );
}

void SrvManager::CreateSRVforTextureCube(
    uint32_t srvIndex,
    ID3D12Resource* resource,
    DXGI_FORMAT format,
    UINT mipLevels)
{
    assert(resource);

    // Skyboxなどで使うCubemap用のSRVを作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = format;
    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.TextureCube.MipLevels = mipLevels;
    srvDesc.TextureCube.MostDetailedMip = 0;
    srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;

    directXCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        GetCPUDescriptorHandle(srvIndex)
    );
}

void SrvManager::CreateSRVforStructuredBuffer(
    uint32_t srvIndex,
    ID3D12Resource* resource,
    UINT numElements,
    UINT structureByteStride)
{
    assert(resource);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.Shader4ComponentMapping =
        D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
    srvDesc.Buffer.NumElements = numElements;
    srvDesc.Buffer.StructureByteStride = structureByteStride;
    srvDesc.Buffer.Flags = D3D12_BUFFER_SRV_FLAG_NONE;

    directXCommon_->GetDevice()->CreateShaderResourceView(
        resource,
        &srvDesc,
        GetCPUDescriptorHandle(srvIndex)
    );
}

void SrvManager::CreateUAVforStructuredBuffer(
    uint32_t srvIndex,
    ID3D12Resource* resource,
    UINT numElements,
    UINT structureByteStride)
{
    assert(resource);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
    uavDesc.Format = DXGI_FORMAT_UNKNOWN;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
    uavDesc.Buffer.FirstElement = 0;
    uavDesc.Buffer.NumElements = numElements;
    uavDesc.Buffer.CounterOffsetInBytes = 0;
    uavDesc.Buffer.Flags = D3D12_BUFFER_UAV_FLAG_NONE;
    uavDesc.Buffer.StructureByteStride = structureByteStride;

    directXCommon_->GetDevice()->CreateUnorderedAccessView(
        resource,
        nullptr,
        &uavDesc,
        GetCPUDescriptorHandle(srvIndex)
    );
}

void SrvManager::PreDraw()
{
    ID3D12DescriptorHeap* heaps[] = { descriptorHeap_.Get() };
    directXCommon_->GetCommandList()->SetDescriptorHeaps(1, heaps);
}

void SrvManager::SetGraphicsRootDescriptorTable(
    UINT rootParameterIndex,
    uint32_t srvIndex)
{
    directXCommon_->GetCommandList()->SetGraphicsRootDescriptorTable(
        rootParameterIndex,
        GetGPUDescriptorHandle(srvIndex)
    );
}

void SrvManager::SetComputeRootDescriptorTable(
    UINT rootParameterIndex,
    uint32_t srvIndex)
{
    directXCommon_->GetCommandList()->SetComputeRootDescriptorTable(
        rootParameterIndex,
        GetGPUDescriptorHandle(srvIndex)
    );
}
