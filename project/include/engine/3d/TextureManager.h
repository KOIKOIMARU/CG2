#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <wrl.h>
#include <d3d12.h>
#include "DirectXTex.h"

class DirectXCommon;
class SrvManager;

// テクスチャのGPU転送とSRV割り当てを共有管理するキャッシュ。
// LoadTexture完了後、同じパスをObject3dやSpriteから安全に参照できる。
class TextureManager {
public:
    static TextureManager* GetInstance();

    // 引数はいずれも借用。Frameworkより先に破棄してはいけない。
    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);

    void LoadTexture(const std::string& filePath, bool forceSrgb = true);
    void CreateSolidTexture2D(
        const std::string& name,
        uint8_t red,
        uint8_t green,
        uint8_t blue,
        uint8_t alpha = 255);
    void CreateSolidCubeTexture(
        const std::string& name,
        uint8_t red,
        uint8_t green,
        uint8_t blue,
        uint8_t alpha = 255);
    void CreateSkyGradientCubeTexture(const std::string& name);

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
        DirectX::TexMetadata metadata;                    // 寸法、形式、ミップ数などの画像情報
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;  // GPU上のテクスチャ本体
        uint32_t srvIndex = 0;                            // シェーダーから参照するSRV番号
    };

    std::unordered_map<std::string, TextureData> textureDatas_; // パスまたは生成名ごとの所有キャッシュ

    DirectXCommon* dxCommon_ = nullptr; // GPU転送に使う描画基盤の借用先
    SrvManager* srvManager_ = nullptr;  // テクスチャSRVを確保する借用先

    static TextureManager* instance_; // アプリ内で共有する唯一の管理インスタンス
};
