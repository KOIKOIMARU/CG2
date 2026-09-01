#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <cstdint>
#include <string>

#include "engine/base/Math.h"

class SpriteCommon;

class Sprite {
public:
    // SpriteCommonは借用し、filePathのテクスチャを使う描画資源を生成する。
    void Initialize(SpriteCommon* spriteCommon, const std::string& filePath);

    void Update();
    void Draw();

    // セッター
    void SetPosition(const Math::Vector2& position) { position_ = position; }
    void SetRotation(float rotation) { rotation_ = rotation; }
    void SetSize(const Math::Vector2& size) { size_ = size; }
    void SetColor(const Math::Vector4& color) { materialData_->color = color; }

    // -------- アンカーポイント --------
    const Math::Vector2& GetAnchorPoint() const { return anchorPoint_; }
    void SetAnchorPoint(const Math::Vector2& p) { anchorPoint_ = p; }

    // -------- UVスケール / 移動 --------
    void SetUVScale(const Math::Vector2& scale) { uvScale_ = scale; }
    const Math::Vector2& GetUVScale() const { return uvScale_; }

    void SetUVTranslate(const Math::Vector2& trans) { uvTranslate_ = trans; }
    const Math::Vector2& GetUVTranslate() const { return uvTranslate_; }

    // -------- フリップ --------
    void SetFlipX(bool flag) { isFlipX_ = flag; }
    void SetFlipY(bool flag) { isFlipY_ = flag; }

    bool GetFlipX() const { return isFlipX_; }
    bool GetFlipY() const { return isFlipY_; }

private:
    struct VertexData {
        Math::Vector4 position; // ローカル座標。wは位置を表す1。
        Math::Vector2 texcoord; // テクスチャ上のUV座標。
        Math::Vector3 normal;   // 2D描画では画面手前を向く固定法線。
    };

    struct Material {
        Math::Vector4 color;         // テクスチャへ乗算するRGBA。
        int32_t enableLighting;      // Spriteでは0固定のライティング有効値。
        float padding[3];            // HLSLの16バイト境界へ合わせる予約領域。
        Math::Matrix4x4 uvTransform; // UVの拡縮・平行移動・反転行列。
    };

    struct TransformData {
        Math::Matrix4x4 WVP;   // ローカル座標から画面へ変換する行列。
        Math::Matrix4x4 World; // ローカル座標からワールドへ変換する行列。
    };

    void CreateVertexData();
    void CreateMaterialData();
    void CreateTransformData();

    // テクスチャサイズから切り出し範囲＆スプライトサイズを初期化
    void AdjustTextureSize();

private:
    // ルートシグネチャ、PSO、DirectX基盤を提供する非所有ポインタ。
    SpriteCommon* spriteCommon_ = nullptr;

    // 四角形の頂点／インデックスと、それらを参照する描画ビュー。
    Microsoft::WRL::ComPtr<ID3D12Resource> vertexResource_;
    Microsoft::WRL::ComPtr<ID3D12Resource> indexResource_;
    VertexData* vertexData_ = nullptr;
    uint32_t* indexData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};
    D3D12_INDEX_BUFFER_VIEW  indexBufferView_{};

    Microsoft::WRL::ComPtr<ID3D12Resource> materialResource_; // マテリアル定数バッファ。
    Material* materialData_ = nullptr; // materialResource_をCPUから更新するマップ先。

    Microsoft::WRL::ComPtr<ID3D12Resource> transformResource_; // 座標変換定数バッファ。
    TransformData* transformData_ = nullptr; // transformResource_の永続マップ先。

    Math::Transform transform_{}; // Update中に組み立てる3D形式の内部変換。

    Math::Vector2 position_{ 0.0f, 0.0f }; // クライアント領域上の表示位置。
    float rotation_ = 0.0f; // Z軸回りの回転角度（ラジアン）。
    Math::Vector2 size_{ 640.0f, 360.0f }; // ピクセル単位の表示幅と高さ。


    std::string textureFilePath_; // TextureManagerへ問い合わせるキャッシュキー。


    // 画像内の回転・配置基準。左上が(0,0)、右下が(1,1)。
    Math::Vector2 anchorPoint_{ 0.0f, 0.0f };

    // UV 切り出し（スケール / 平行移動）
    Math::Vector2 uvScale_ = { 1.0f, 1.0f };
    Math::Vector2 uvTranslate_ = { 0.0f, 0.0f };

    // 反転フラグ
    bool isFlipX_ = false;
    bool isFlipY_ = false;
};
