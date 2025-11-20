#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>

#include "engine/base/Math.h"

class SpriteCommon;

class Sprite {
public:
    void Initialize(SpriteCommon* spriteCommon);
    void Update();
    void Draw();

    // セッター
    void SetTexture(D3D12_GPU_DESCRIPTOR_HANDLE handle) { textureSrvHandle_ = handle; }

    const Math::Vector2& GetPosition() const { return position_; }
    void SetPosition(const Math::Vector2& position) { position_ = position; }

    float GetRotation() const { return rotation_; }
	void SetRotation(float rotation) { this->rotation_ = rotation; }

    const Math::Vector4& GetColor() const { return materialData_->color; }
    void SetColor(const Math::Vector4& color) { materialData_->color = color; }

    const Math::Vector2& GetSize() const { return size_; }
    void SetSize(const Math::Vector2& size) { size_ = size; }

private:
    // 頂点データ
    struct VertexData {
        Math::Vector4 position;
        Math::Vector2 texcoord;
        Math::Vector3 normal;
    };

    // マテリアルデータ
    struct Material {
        Math::Vector4  color;
        int32_t        enableLighting;
        float          padding[3];
        Math::Matrix4x4 uvTransform;
    };

    // Transform用定数バッファ
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

    Math::Transform transform_{};   // 2D用 Transform

    D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandle_{}; // t0

    Math::Vector2 position_{ 0.0f, 0.0f };
	float rotation_ = 0.0f;
    Math::Vector2 size_{ 640.0f, 360.0f };
};
