#include "engine/base/SrvManager.h"
#include "engine/base/DirectXCommon.h"
#include <cassert>
#include <stdexcept>

using Microsoft::WRL::ComPtr;

void SrvManager::Initialize(DirectXCommon* dxCommon)
{
    if (!dxCommon) {
        throw std::invalid_argument("SrvManager::Initialize requires DirectXCommon");
    }
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
    allocated_.fill(false);
}

uint32_t SrvManager::Allocate()
{
    if (!freeIndices_.empty()) {
        uint32_t index = freeIndices_.back();
        freeIndices_.pop_back();
        assert(index < allocated_.size() && !allocated_[index]);
        allocated_[index] = true;
        return index;
    }

    if (useIndex_ >= kMaxSRVCount) {
        throw std::runtime_error("SRV descriptor heap exhausted");
    }

    uint32_t index = useIndex_;
    useIndex_++;
    allocated_[index] = true;
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
    if (index >= useIndex_ || index >= allocated_.size() || !allocated_[index]) {
        assert(false && "Attempted to free an invalid SRV descriptor index");
        return;
    }
    allocated_[index] = false;
    freeIndices_.push_back(index);
}

void SrvManager::Free(D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
    if (!descriptorHeap_ || descriptorSize_ == 0) {
        throw std::logic_error("SrvManager is not initialized");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE start =
        descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    if (handle.ptr < start.ptr) {
        throw std::invalid_argument("SRV handle is outside the managed heap");
    }

    SIZE_T offset = handle.ptr - start.ptr;
    if (offset % descriptorSize_ != 0) {
        throw std::invalid_argument("SRV handle is not descriptor-aligned");
    }

    uint32_t index = static_cast<uint32_t>(offset / descriptorSize_);
    Free(index);
}

D3D12_CPU_DESCRIPTOR_HANDLE
SrvManager::GetCPUDescriptorHandle(uint32_t index)
{
    if (!descriptorHeap_) {
        throw std::logic_error("SrvManager is not initialized");
    }
    if (index >= kMaxSRVCount) {
        throw std::out_of_range("SRV descriptor index is out of range");
    }

    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        descriptorHeap_->GetCPUDescriptorHandleForHeapStart();
    handle.ptr += static_cast<SIZE_T>(descriptorSize_) * index;
    return handle;
}

D3D12_GPU_DESCRIPTOR_HANDLE
SrvManager::GetGPUDescriptorHandle(uint32_t index)
{
    if (!descriptorHeap_) {
        throw std::logic_error("SrvManager is not initialized");
    }
    if (index >= kMaxSRVCount) {
        throw std::out_of_range("SRV descriptor index is out of range");
    }

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
    if (!resource) {
        throw std::invalid_argument("Texture2D SRV requires a resource");
    }

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
    if (!resource) {
        throw std::invalid_argument("TextureCube SRV requires a resource");
    }

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
    if (!resource) {
        throw std::invalid_argument("Structured buffer SRV requires a resource");
    }

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
    if (!resource) {
        throw std::invalid_argument("Structured buffer UAV requires a resource");
    }

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
