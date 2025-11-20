#include "engine/2d/Sprite.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/base/DirectXCommon.h"
#include <cassert>

using Microsoft::WRL::ComPtr;
using namespace Math;

void Sprite::Initialize(SpriteCommon* spriteCommon)
{
    assert(spriteCommon);
    spriteCommon_ = spriteCommon;

    CreateVertexData();
    CreateMaterialData();
    CreateTransformData();
}

void Sprite::Update()
{
    transform_.translate = { position_.x, position_.y, 0.0f };
	transform_.rotate = { 0.0f, 0.0f, rotation_ };
    transform_.scale = { size_.x, size_.y, 1.0f };

    Matrix4x4 world = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate);

    Matrix4x4 view = MakeIdentity4x4();
    Matrix4x4 proj = MakeIdentity4x4();
    Matrix4x4 vp = Multiply(view, proj);

    transformData_->World = world;
    transformData_->WVP = Multiply(world, vp);
}

void Sprite::Draw()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();
    ID3D12GraphicsCommandList* commandList = dxCommon->GetCommandList();

    commandList->IASetVertexBuffers(0, 1, &vertexBufferView_);
    commandList->IASetIndexBuffer(&indexBufferView_);

    commandList->SetGraphicsRootConstantBufferView(
        0, materialResource_->GetGPUVirtualAddress());
    commandList->SetGraphicsRootConstantBufferView(
        1, transformResource_->GetGPUVirtualAddress());

    if (textureSrvHandle_.ptr != 0) {
        commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandle_);
    }

    commandList->DrawIndexedInstanced(6, 1, 0, 0, 0);
}

void Sprite::CreateVertexData()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    vertexResource_ = dxCommon->CreateBufferResource(sizeof(VertexData) * 4);
    indexResource_ = dxCommon->CreateBufferResource(sizeof(uint32_t) * 6);

    // 頂点データ
    vertexResource_->Map(0, nullptr, reinterpret_cast<void**>(&vertexData_));

    // 左下
    vertexData_[0].position = { 0.0f, 1.0f, 0.0f, 1.0f };
    vertexData_[0].texcoord = { 0.0f, 1.0f };
    vertexData_[0].normal = { 0.0f, 0.0f, -1.0f };

    // 左上
    vertexData_[1].position = { 0.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[1].texcoord = { 0.0f, 0.0f };
    vertexData_[1].normal = { 0.0f, 0.0f, -1.0f };

    // 右下
    vertexData_[2].position = { 1.0f, 1.0f, 0.0f, 1.0f };
    vertexData_[2].texcoord = { 1.0f, 1.0f };
    vertexData_[2].normal = { 0.0f, 0.0f, -1.0f };

    // 右上
    vertexData_[3].position = { 1.0f, 0.0f, 0.0f, 1.0f };
    vertexData_[3].texcoord = { 1.0f, 0.0f };
    vertexData_[3].normal = { 0.0f, 0.0f, -1.0f };

    vertexResource_->Unmap(0, nullptr);
    vertexData_ = nullptr;

    // インデックス
    indexResource_->Map(0, nullptr, reinterpret_cast<void**>(&indexData_));
    indexData_[0] = 0; indexData_[1] = 1; indexData_[2] = 2;
    indexData_[3] = 1; indexData_[4] = 3; indexData_[5] = 2;
    indexResource_->Unmap(0, nullptr);
    indexData_ = nullptr;

    // VB/IBビュー
    vertexBufferView_.BufferLocation = vertexResource_->GetGPUVirtualAddress();
    vertexBufferView_.SizeInBytes = sizeof(VertexData) * 4;
    vertexBufferView_.StrideInBytes = sizeof(VertexData);

    indexBufferView_.BufferLocation = indexResource_->GetGPUVirtualAddress();
    indexBufferView_.SizeInBytes = sizeof(uint32_t) * 6;
    indexBufferView_.Format = DXGI_FORMAT_R32_UINT;
}


void Sprite::CreateMaterialData()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    materialResource_ = dxCommon->CreateBufferResource(sizeof(Material));
    materialResource_->Map(0, nullptr, reinterpret_cast<void**>(&materialData_));

    materialData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    materialData_->enableLighting = false;
    materialData_->padding[0] = materialData_->padding[1] = materialData_->padding[2] = 0.0f;
    materialData_->uvTransform = MakeIdentity4x4();
}

void Sprite::CreateTransformData()
{
    DirectXCommon* dxCommon = spriteCommon_->GetDxCommon();

    transformResource_ = dxCommon->CreateBufferResource(sizeof(TransformData));
    transformResource_->Map(0, nullptr, reinterpret_cast<void**>(&transformData_));

    transform_.scale = { 1.0f, 1.0f, 1.0f };
    transform_.rotate = { 0.0f, 0.0f, 0.0f };
    transform_.translate = { 0.0f, 0.0f, 0.0f };

    Matrix4x4 world = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate);

    Matrix4x4 vp = MakeIdentity4x4();
    transformData_->World = world;
    transformData_->WVP = Multiply(world, vp);
}
