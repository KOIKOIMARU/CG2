#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include <cassert>
#include <cstdint>

TextureManager* TextureManager::instance_ = nullptr;

TextureManager* TextureManager::GetInstance() {
    if (!instance_) {
        instance_ = new TextureManager();
    }
    return instance_;
}

void TextureManager::Initialize(
    DirectXCommon* dxCommon,
    SrvManager* srvManager) {

    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    textureDatas_.clear();
}

void TextureManager::Destroy() {
    delete instance_;
    instance_ = nullptr;
}

void TextureManager::LoadTexture(const std::string& filePath) {

    // ★ 読み込み済みチェック（unordered_map）
    if (textureDatas_.contains(filePath)) {
        return;
    }




    TextureData& textureData = textureDatas_[filePath];

    // 読み込み
    DirectX::ScratchImage mipImages =
        DirectXCommon::LoadTexture(filePath);
    textureData.metadata = mipImages.GetMetadata();

    // GPUリソース作成
    textureData.resource =
        dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, mipImages);

    // ===== SRV確保 =====
    textureData.srvIndex = srvManager_->Allocate();

    // Cubemapの場合はTexture2DではなくTextureCubeとしてSRVを作成
    if (textureData.metadata.IsCubemap()) {
        srvManager_->CreateSRVforTextureCube(
            textureData.srvIndex,
            textureData.resource.Get(),
            textureData.metadata.format,
            UINT(textureData.metadata.mipLevels)
        );
    } else {
        // SRV作成（Texture2D）
        srvManager_->CreateSRVforTexture2D(
            textureData.srvIndex,
            textureData.resource.Get(),
            textureData.metadata.format,
            UINT(textureData.metadata.mipLevels)
        );
    }

}

void TextureManager::CreateSolidCubeTexture(
    const std::string& name,
    uint8_t red,
    uint8_t green,
    uint8_t blue,
    uint8_t alpha)
{
    if (textureDatas_.contains(name)) {
        return;
    }

    DirectX::ScratchImage image;
    HRESULT hr = image.InitializeCube(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        1,
        1,
        1,
        1);
    assert(SUCCEEDED(hr));

    const DirectX::Image* images = image.GetImages();
    for (size_t index = 0; index < image.GetImageCount(); ++index) {
        uint8_t* pixel = images[index].pixels;
        pixel[0] = red;
        pixel[1] = green;
        pixel[2] = blue;
        pixel[3] = alpha;
    }

    TextureData& textureData = textureDatas_[name];
    textureData.metadata = image.GetMetadata();
    textureData.resource =
        dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, image);

    textureData.srvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforTextureCube(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metadata.format,
        UINT(textureData.metadata.mipLevels));
}

const DirectX::TexMetadata&
TextureManager::GetMetaData(const std::string& filePath) {
    assert(textureDatas_.contains(filePath));
    return textureDatas_.at(filePath).metadata;
}

uint32_t TextureManager::GetSrvIndex(const std::string& filePath) {
    assert(textureDatas_.contains(filePath));
    return textureDatas_.at(filePath).srvIndex;
}
