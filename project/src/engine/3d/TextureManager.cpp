#include "engine/3d/TextureManager.h"
#include <algorithm>
#include <cassert>

TextureManager* TextureManager::instance_ = nullptr;

// ImGui 分を避けるための開始インデックス
uint32_t TextureManager::kSRVIndexTop = 1;

TextureManager* TextureManager::GetInstance()
{
    if (instance_ == nullptr) {
        instance_ = new TextureManager();
    }
    return instance_;
}

void TextureManager::Initialize(ID3D12Device* device, DirectXCommon* dxCommon)
{
    device_ = device;
    dxCommon_ = dxCommon;

    textureDatas_.clear();
}

void TextureManager::Finalize()
{
    delete instance_;
    instance_ = nullptr;
}

void TextureManager::LoadTexture(const std::string& filePath)
{
    // 重複チェック
    auto it = std::find_if(
        textureDatas_.begin(),
        textureDatas_.end(),
        [&](const TextureData& data) {
            return data.filePath == filePath;
        }
    );

    if (it != textureDatas_.end()) {
        return;
    }

    // 新規データ追加
    textureDatas_.resize(textureDatas_.size() + 1);
    TextureData& textureData = textureDatas_.back();

    // 読み込み
    DirectX::ScratchImage mipImages = DirectXCommon::LoadTexture(filePath);
    textureData.metadata = mipImages.GetMetadata();
    textureData.filePath = filePath;

    // GPU リソース作成
    textureData.resource = dxCommon_->CreateTextureResource(textureData.metadata);
    dxCommon_->UploadTextureData(textureData.resource, mipImages);

    // SRV ハンドル割り当て
    uint32_t textureIndex = static_cast<uint32_t>(textureDatas_.size() - 1);
    uint32_t srvIndex = textureIndex + kSRVIndexTop;

    assert(textureIndex + kSRVIndexTop < DirectXCommon::kMaxSRVCount);

    textureData.srvHandleCPU = dxCommon_->GetSRVCPUDescriptorHandle(srvIndex);
    textureData.srvHandleGPU = dxCommon_->GetSRVGPUDescriptorHandle(srvIndex);

    // SRV 作成
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = textureData.metadata.format;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = UINT(textureData.metadata.mipLevels);

    device_->CreateShaderResourceView(
        textureData.resource.Get(),
        &srvDesc,
        textureData.srvHandleCPU);
}

D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetTextureHandle(const std::string& filePath)
{
    // まず検索
    auto it = std::find_if(
        textureDatas_.begin(),
        textureDatas_.end(),
        [&](const TextureData& data) {
            return data.filePath == filePath;
        }
    );

    // 無ければ読み込む
    if (it == textureDatas_.end()) {
        LoadTexture(filePath);

        it = std::find_if(
            textureDatas_.begin(),
            textureDatas_.end(),
            [&](const TextureData& data) {
                return data.filePath == filePath;
            }
        );

        if (it == textureDatas_.end()) {
            // 読み込み失敗時は0番を返す
            return dxCommon_->GetSRVGPUDescriptorHandle(0);
        }
    }

    uint32_t textureIndex =
        static_cast<uint32_t>(std::distance(textureDatas_.begin(), it));
    return GetSrvHandleGPU(textureIndex);
}

// 追加：ファイルパスからテクスチャ番号取得
uint32_t TextureManager::GetTextureIndexByFilePath(const std::string& filePath)
{
    auto it = std::find_if(
        textureDatas_.begin(),
        textureDatas_.end(),
        [&](const TextureData& data) {
            return data.filePath == filePath;
        }
    );

    if (it != textureDatas_.end()) {
        uint32_t textureIndex =
            static_cast<uint32_t>(std::distance(textureDatas_.begin(), it));
        return textureIndex;
    }

    assert(false);   // 読み込んでないテクスチャ
    return 0;
}

// 追加：テクスチャ番号からGPUハンドル取得
D3D12_GPU_DESCRIPTOR_HANDLE TextureManager::GetSrvHandleGPU(uint32_t textureIndex)
{
    assert(textureIndex < textureDatas_.size());
    TextureData& textureData = textureDatas_[textureIndex];
    return textureData.srvHandleGPU;
}

const DirectX::TexMetadata& TextureManager::GetMetaData(uint32_t textureIndex)
{
    assert(textureIndex < textureDatas_.size());
    return textureDatas_[textureIndex].metadata;
}
