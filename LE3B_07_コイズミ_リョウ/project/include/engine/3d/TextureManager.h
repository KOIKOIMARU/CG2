#pragma once
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXTex.h"

class DirectXCommon;
class SrvManager;

class TextureManager {
public:
    static TextureManager* GetInstance();

    // ★ SRVマネージャを受け取る
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    void LoadTexture(const std::string& filePath);

    // ===== getter =====
    const DirectX::TexMetadata& GetMetaData(const std::string& filePath);
    uint32_t GetSrvIndex(const std::string& filePath);

    static void Destroy();

private:
    TextureManager() = default;
    ~TextureManager() = default;
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

private:
    struct TextureData {
        DirectX::TexMetadata metadata;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        uint32_t srvIndex = 0;
    };


    std::unordered_map<std::string, TextureData> textureDatas_;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;

    static TextureManager* instance_;
};
