#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>          // ★ 追加！

#include "engine/base/Math.h"

class SpriteCommon;

class Sprite {
public:
    // ★ 追加：ファイルパスで初期化
    void Initialize(SpriteCommon* spriteCommon, const std::string& filePath);

    void Update();
    void Draw();

    // 今後は SetTexture を使わない（削除推奨）
    // void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE handle);

    // セッター
    void SetPosition(const Math::Vector2& position) { position_ = position; }
    void SetRotation(float rotation) { rotation_ = rotation; }
    void SetSize(const Math::Vector2& size) { size_ = size; }
    void SetColor(const Math::Vector4& color) { materialData_->color = color; }

private:
    struct VertexData {
        Math::Vector4 position;
        Math::Vector2 texcoord;
        Math::Vector3 normal;
    };

    struct Material {
        Math::Vector4  color;
        int32_t        enableLighting;
        float          padding[3];
        Math::Matrix4x4 uvTransform;
    };

    struct TransformData {
        Math::Matrix4x4 WVP;
        Math::Matrix4x4 World;
    };

    void CreateVertexData();
    void CreateMaterialData();
    void CreateTransformData();

private:
    SpriteCommon* spriteCommon_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW  indexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_;
    TransformData* transformData_ = nullptr;

    Math::Transform transform_{};

    Math::Vector2 position_{ 0.0f, 0.0f };
    float rotation_ = 0.0f;
    Math::Vector2 size_{ 640.0f, 360.0f };

    // ★ TextureManager 経由の GPUハンドルだけ残す
    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle_{};
};
