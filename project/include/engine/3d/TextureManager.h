#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXTex.h"
#include "engine/base/DirectXCommon.h"

class TextureManager
{
public:
    static TextureManager* GetInstance();

    void Initialize(ID3D12Device* device, DirectXCommon* dxCommon);
    void Finalize();

    void LoadTexture(const std::string& filePath);

    // 文字列から直接ハンドルを取る従来関数（3Dなど用に残す）
    D3D12_GPU_DESCRIPTOR_HANDLE GetTextureHandle(const std::string& filePath);

    // 追加：ファイルパスからテクスチャ番号を取得
    uint32_t GetTextureIndexByFilePath(const std::string& filePath);

    // 追加：テクスチャ番号からGPUハンドルを取得
    D3D12_GPU_DESCRIPTOR_HANDLE GetSrvHandleGPU(uint32_t textureIndex);

    const DirectX::TexMetadata& GetMetaData(uint32_t textureIndex);

private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(TextureManager&) = delete;
    TextureManager& operator=(TextureManager&) = delete;

private:
    struct TextureData {
        std::string filePath;
        DirectX::TexMetadata metadata;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_CPU_DESCRIPTOR_HANDLE srvHandleCPU{};
        D3D12_GPU_DESCRIPTOR_HANDLE srvHandleGPU{};
    };

    std::vector<TextureData> textureDatas_;

    ID3D12Device* device_ = nullptr;
    DirectXCommon* dxCommon_ = nullptr;

    static TextureManager* instance_;

    // SRVインデックスの開始番号
    static uint32_t kSRVIndexTop;
};
