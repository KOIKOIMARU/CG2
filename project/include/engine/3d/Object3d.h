#pragma once
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"
#include "engine/3d/Model.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

class Object3dCommon;
class Camera;

// Object3dと各シェーダー間で共有する定数・構造化バッファのレイアウト。
// メンバー順やpaddingを変更する場合は対応するHLSL構造体も同時に変更する。
struct TransformationMatrix {
    Matrix4x4 WVP;                   // ローカル座標からクリップ座標までの変換
    Matrix4x4 World;                 // ローカル座標からワールド座標への変換
    Matrix4x4 WorldInverseTranspose; // 法線をワールド空間へ変換する行列
};

struct DirectionalLight {
    Vector4 color;                   // 平行光源のRGBA色
    Vector3 direction;               // 光が進むワールド空間方向
    float intensity;                 // 光量
    Matrix4x4 lightViewProjection;   // 影マップへ投影するための行列
    float shadowStrength;            // 影を暗くする割合
    float shadowBias;                // 深度比較時の固定ずれ補正
    float shadowNormalBias;          // 面の向きを考慮した影ずれ補正
    float shadowMapEnabled;          // 影マップが有効なら1、無効なら0
};

struct CameraForGPU {
    Vector3 worldPosition; // 鏡面反射の視線計算に使うカメラ位置
    float padding;         // 16バイト境界にそろえる余白
};

struct PointLight {
    Vector4 color;     // 点光源のRGBA色
    Vector3 position;  // ワールド空間上の発光位置
    float intensity;   // 光量
    float radius;      // 光が届く最大距離
    float decay;       // 距離減衰の指数
    float padding[2];  // 16バイト境界にそろえる余白
};

struct SpotLight {
    Vector4 color;        // スポットライトのRGBA色
    Vector3 position;     // ワールド空間上の発光位置
    float intensity;      // 光量
    Vector3 direction;    // 光が進むワールド空間方向
    float distance;       // 光が届く最大距離
    float decay;          // 距離減衰の指数
    float cosAngle;       // 照射範囲外端の角度余弦
    float cosFalloffStart;// 減衰を始める内側角度の余弦
    float padding;        // 16バイト境界にそろえる余白
};

struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;                 // 頂点位置を現在のボーン姿勢へ動かす行列
    Matrix4x4 skeletonSpaceInverseTransposeMatrix; // 頂点法線を現在姿勢へ動かす行列
};

constexpr uint32_t kNumMaxSkeletonJoints = 128;

struct SkinningPaletteForGPU {
    int32_t enableSkinning;                         // スキニングを適用する場合は1
    float padding[3];                               // 16バイト境界にそろえる余白
    WellForGPU palette[kNumMaxSkeletonJoints];      // ジョイント番号で参照する変形行列
};

struct SkinningVertexForCompute {
    Vector4 position; // Compute Shaderへ渡す変形前の頂点位置
    Vector2 texcoord; // 変形せず出力へ引き継ぐUV座標
    Vector3 normal;   // Compute Shaderへ渡す変形前の法線
};

struct VertexInfluenceForCompute {
    Vector4 weight;                                        // 頂点に影響する各ジョイントの比率
    std::array<int32_t, kNumMaxInfluence> jointIndices;    // 各比率に対応するジョイント番号
};

struct SkinningInformationForCompute {
    uint32_t numVertices; // Compute Shaderが処理する頂点数
    float padding[3];     // 16バイト境界にそろえる余白
};

// 3Dモデル1個分の変換、マテリアル、ライト、アニメーション状態を保持する描画単位。
// Modelの所有権は持たない。Initialize後にSetModelを行い、毎フレームUpdateしてからDrawする。
class Object3d {
public:
    // Object3dCommonは借用。Initialize後にモデルとカメラを設定する。
    void Initialize(Object3dCommon* object3dCommon);
    // CPU状態をGPU定数へ反映する。Drawより先に毎フレーム呼ぶ。
    void Update();
    void Draw();
    void DrawShadow(const Matrix4x4& lightViewProjection);
    void UpdateAnimation(float deltaTime);

    void SetModel(Model* model);

    // 変換・ライト・マテリアルの上書き値を設定する。
    void SetScale(const Vector3& scale);
    void SetRotate(const Vector3& rotate);
    void SetQuaternionRotate(const Quaternion& rotate);
    void SetTranslate(const Vector3& translate);
    void SetDirectionalLightDirection(const Vector3& direction);
    void SetDirectionalLightIntensity(float intensity);
    void SetPointLightPosition(const Vector3& position);
    void SetPointLightIntensity(float intensity);
    void SetSpotLightPosition(const Vector3& position);
    void SetSpotLightDirection(const Vector3& direction);
    void SetSpotLightIntensity(float intensity);
    void SetEnvironmentCoefficient(float coefficient);
    void SetColor(const Vector4& color);
    void SetShininess(float shininess);
    void SetSpecularColor(const Vector3& color);
    void SetRoughness(float roughness);
    void SetMetallic(float metallic);
    void SetAlphaReference(float alphaReference);
    void SetShadowReceiveStrength(float strength);
    void SetUVTransform(const Matrix4x4& uvTransform);
    void SetLightingMode(int32_t lightingMode);
    void SetTextureFilePath(const std::string& textureFilePath);
    bool HasSkeleton() const { return hasSkeleton_; }
    const Skeleton& GetSkeleton() const { return skeleton_; }
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    // 現在のCPU側設定値を返す。
    Vector3 GetScale() const;
    Vector3 GetRotate() const;
    Vector3 GetTranslate() const;
    float GetEnvironmentCoefficient() const;
    Vector4 GetColor() const;
    float GetShininess() const;
    Vector3 GetSpecularColor() const;
    float GetRoughness() const;
    float GetMetallic() const;
    float GetAlphaReference() const;
    float GetShadowReceiveStrength() const { return shadowReceiveStrength_; }
    int32_t GetLightingMode() const;
    const std::string& GetTextureFilePath() const;

    void SetModel(const std::string& filePath);

    // カメラは借用。Object3dの描画中まで生存させる。
    void SetCamera(Camera* camera) { this->camera_ = camera; }


private:
    // 初期化時のGPU資源生成と、スキニング更新処理。
    void CreateTransformationMatrix();
    void CreateDirectionalLight();
    void CreateCameraResource();
    void CreatePointLight();
    void CreateSpotLight();
    void CreateMaterialOverride();
    void CreateSkinningPalette();
    void CreateComputeSkinningPipeline();
    void InitializeSkinning();
    void UpdateSkinningPalette();
    void InitializeComputeSkinningResources();
    void ReleaseComputeSkinningResources();
    void DispatchComputeSkinning();

private:
    Object3dCommon* object3dCommon_ = nullptr; // 共通描画設定を参照する借用先
    Model* model_ = nullptr;                   // 描画するモデル。所有権はModelManager側

    Transform transform_;                                      // オブジェクトの拡縮・オイラー回転・位置
    Quaternion quaternionRotate_{ 0.0f, 0.0f, 0.0f, 1.0f };    // クォータニオン指定時の回転
    bool useQuaternionRotate_ = false;                          // trueならtransform_の回転より上記を優先
    Camera* camera_ = nullptr;                                  // 描画に使うカメラの借用先
    Matrix4x4 worldMatrix_ = MakeIdentity4x4();                  // Updateで確定したワールド行列

    ComPtr<ID3D12Resource> transformationMatrixResource_; // 変換行列の定数バッファ
    TransformationMatrix* transformationMatrixData_ = nullptr; // 上記バッファのCPU書込み先

    ComPtr<ID3D12Resource> directionalLightResource_; // 平行光源の定数バッファ
    DirectionalLight* directionalLightData_ = nullptr;// 上記バッファのCPU書込み先
    float shadowReceiveStrength_ = 1.0f;              // この物体だけに適用する影の受け具合

    ComPtr<ID3D12Resource> cameraResource_; // カメラ位置の定数バッファ
    CameraForGPU* cameraData_ = nullptr;    // 上記バッファのCPU書込み先

    ComPtr<ID3D12Resource> pointLightResource_; // 点光源の定数バッファ
    PointLight* pointLightData_ = nullptr;       // 上記バッファのCPU書込み先

    ComPtr<ID3D12Resource> spotLightResource_; // スポットライトの定数バッファ
    SpotLight* spotLightData_ = nullptr;       // 上記バッファのCPU書込み先

    ComPtr<ID3D12Resource> materialResource_; // オブジェクト固有マテリアルの定数バッファ
    Material* materialData_ = nullptr;        // 上記バッファのCPU書込み先
    std::string textureFilePath_;             // モデル設定を上書きするテクスチャ
    bool hasTextureOverride_ = false;         // 個別テクスチャを使う場合はtrue

    ComPtr<ID3D12Resource> skinningPaletteResource_;  // Vertex Shader用のボーン行列定数
    SkinningPaletteForGPU* skinningPaletteData_ = nullptr; // 上記バッファのCPU書込み先
    ComPtr<ID3D12RootSignature> computeRootSignature_;// Compute Skinning用ルートシグネチャ
    ComPtr<ID3D12PipelineState> computePipelineState_;// Compute Skinning用PSO
    ComPtr<ID3D12Resource> computeInputVertexResource_;// 変形前頂点のStructured Buffer
    ComPtr<ID3D12Resource> computeInfluenceResource_; // 頂点ごとのボーン影響情報
    ComPtr<ID3D12Resource> computeMatrixPaletteResource_; // Compute Shader用ボーン行列
    ComPtr<ID3D12Resource> computeOutputVertexResource_;  // 変形後頂点を書き出すUAV
    ComPtr<ID3D12Resource> computeSkinningInfoResource_;  // 処理頂点数の定数バッファ
    WellForGPU* computeMatrixPaletteData_ = nullptr;      // ボーン行列バッファの書込み先
    SkinningInformationForCompute* computeSkinningInfoData_ = nullptr; // 頂点数バッファの書込み先
    D3D12_VERTEX_BUFFER_VIEW computeOutputVertexBufferView_{}; // 変形後頂点の描画ビュー
    D3D12_RESOURCE_STATES computeOutputVertexState_ = D3D12_RESOURCE_STATE_COMMON; // 出力頂点の現在状態
    uint32_t computePaletteSrvIndex_ = UINT32_MAX;         // ボーン行列SRVのヒープ番号
    uint32_t computeInputVertexSrvIndex_ = UINT32_MAX;     // 変形前頂点SRVのヒープ番号
    uint32_t computeInfluenceSrvIndex_ = UINT32_MAX;       // ボーン影響SRVのヒープ番号
    uint32_t computeOutputVertexUavIndex_ = UINT32_MAX;    // 変形後頂点UAVのヒープ番号
    bool enableComputeSkinning_ = false;                   // Compute Shader経路を使うか
    Skeleton skeleton_{};                                  // 現在のボーン階層と姿勢
    std::vector<Matrix4x4> inverseBindPoseMatrices_;       // ジョイント番号順の逆バインド行列
    bool hasSkeleton_ = false;                             // モデルがボーンを持つか
    float animationTime_ = 0.0f;                           // 現在の再生時刻（秒）
};
