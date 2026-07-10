#include "engine/3d/TextureManager.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include <cassert>
#include <algorithm>
#include <cmath>
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
    CreateSolidTexture2D("resources/default_normal.png", 128, 128, 255, 255);
}

void TextureManager::Destroy() {
    delete instance_;
    instance_ = nullptr;
}

void TextureManager::LoadTexture(const std::string& filePath, bool forceSrgb) {

    // ★ 読み込み済みチェック（unordered_map）
    if (textureDatas_.contains(filePath)) {
        return;
    }




    TextureData& textureData = textureDatas_[filePath];

    // 読み込み
    DirectX::ScratchImage mipImages =
        DirectXCommon::LoadTexture(filePath, forceSrgb);
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

void TextureManager::CreateSolidTexture2D(
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
    HRESULT hr = image.Initialize2D(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        1,
        1,
        1,
        1);
    assert(SUCCEEDED(hr));

    uint8_t* pixel = image.GetPixels();
    pixel[0] = red;
    pixel[1] = green;
    pixel[2] = blue;
    pixel[3] = alpha;

    TextureData& textureData = textureDatas_[name];
    textureData.metadata = image.GetMetadata();
    textureData.resource =
        dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, image);

    textureData.srvIndex = srvManager_->Allocate();
    srvManager_->CreateSRVforTexture2D(
        textureData.srvIndex,
        textureData.resource.Get(),
        textureData.metadata.format,
        UINT(textureData.metadata.mipLevels));
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

void TextureManager::CreateSkyGradientCubeTexture(const std::string& name)
{
    if (textureDatas_.contains(name)) {
        return;
    }

    constexpr size_t kSize = 16;
    DirectX::ScratchImage image;
    HRESULT hr = image.InitializeCube(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        kSize,
        kSize,
        1,
        1);
    assert(SUCCEEDED(hr));

    const auto writePixel =
        [](uint8_t* pixel, float red, float green, float blue) {
            pixel[0] = static_cast<uint8_t>(std::clamp(red, 0.0f, 255.0f));
            pixel[1] = static_cast<uint8_t>(std::clamp(green, 0.0f, 255.0f));
            pixel[2] = static_cast<uint8_t>(std::clamp(blue, 0.0f, 255.0f));
            pixel[3] = 255;
        };
    const auto mix = [](float a, float b, float rate) {
        return a + (b - a) * std::clamp(rate, 0.0f, 1.0f);
    };

    const DirectX::Image* images = image.GetImages();
    for (size_t face = 0; face < image.GetImageCount(); ++face) {
        DirectX::Image faceImage = images[face];
        for (size_t y = 0; y < kSize; ++y) {
            uint8_t* row = faceImage.pixels + faceImage.rowPitch * y;
            for (size_t x = 0; x < kSize; ++x) {
                const float v =
                    (static_cast<float>(y) + 0.5f) / static_cast<float>(kSize) *
                    2.0f - 1.0f;
                float dirY = -v;
                if (face == 2) {
                    dirY = 1.0f;
                } else if (face == 3) {
                    dirY = -1.0f;
                }

                const float skyRate = std::clamp((dirY + 0.15f) * 0.62f, 0.0f, 1.0f);
                const float horizonGlow =
                    (std::max)(0.0f, 1.0f - std::abs(dirY + 0.12f) * 3.1f);
                const float cloudBand =
                    (std::max)(0.0f, 1.0f - std::abs(dirY + 0.36f) * 4.4f);

                float red = mix(86.0f, 156.0f, skyRate);
                float green = mix(126.0f, 190.0f, skyRate);
                float blue = mix(184.0f, 232.0f, skyRate);
                red = mix(red, 225.0f, horizonGlow * 0.34f);
                green = mix(green, 235.0f, horizonGlow * 0.34f);
                blue = mix(blue, 246.0f, horizonGlow * 0.34f);
                red = mix(red, 208.0f, cloudBand * 0.20f);
                green = mix(green, 224.0f, cloudBand * 0.20f);
                blue = mix(blue, 238.0f, cloudBand * 0.20f);

                writePixel(row + x * 4, red, green, blue);
            }
        }
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
