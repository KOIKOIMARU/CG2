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
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
    Matrix4x4 lightViewProjection;
    float shadowStrength;
    float shadowBias;
    float shadowNormalBias;
    float shadowMapEnabled;
};

struct CameraForGPU {
    Vector3 worldPosition;
    float padding;
};

struct PointLight {
    Vector4 color;
    Vector3 position;
    float intensity;
    float radius;
    float decay;
    float padding[2];
};

struct SpotLight {
    Vector4 color;
    Vector3 position;
    float intensity;
    Vector3 direction;
    float distance;
    float decay;
    float cosAngle;
    float cosFalloffStart;
    float padding;
};

struct WellForGPU {
    Matrix4x4 skeletonSpaceMatrix;
    Matrix4x4 skeletonSpaceInverseTransposeMatrix;
};

constexpr uint32_t kNumMaxSkeletonJoints = 128;

struct SkinningPaletteForGPU {
    int32_t enableSkinning;
    float padding[3];
    WellForGPU palette[kNumMaxSkeletonJoints];
};

struct SkinningVertexForCompute {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct VertexInfluenceForCompute {
    Vector4 weight;
    std::array<int32_t, kNumMaxInfluence> jointIndices;
};

struct SkinningInformationForCompute {
    uint32_t numVertices;
    float padding[3];
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
    Object3dCommon* object3dCommon_ = nullptr;
    Model* model_ = nullptr;

    Transform transform_;
    Quaternion quaternionRotate_{ 0.0f, 0.0f, 0.0f, 1.0f };
    bool useQuaternionRotate_ = false;
    Camera* camera_ = nullptr;
    Matrix4x4 worldMatrix_ = MakeIdentity4x4();

    ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;
    float shadowReceiveStrength_ = 1.0f;

    ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* cameraData_ = nullptr;

    ComPtr<ID3D12Resource> pointLightResource_;
    PointLight* pointLightData_ = nullptr;

    ComPtr<ID3D12Resource> spotLightResource_;
    SpotLight* spotLightData_ = nullptr;

    ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
    std::string textureFilePath_;
    bool hasTextureOverride_ = false;

    ComPtr<ID3D12Resource> skinningPaletteResource_;
    SkinningPaletteForGPU* skinningPaletteData_ = nullptr;
    ComPtr<ID3D12RootSignature> computeRootSignature_;
    ComPtr<ID3D12PipelineState> computePipelineState_;
    ComPtr<ID3D12Resource> computeInputVertexResource_;
    ComPtr<ID3D12Resource> computeInfluenceResource_;
    ComPtr<ID3D12Resource> computeMatrixPaletteResource_;
    ComPtr<ID3D12Resource> computeOutputVertexResource_;
    ComPtr<ID3D12Resource> computeSkinningInfoResource_;
    WellForGPU* computeMatrixPaletteData_ = nullptr;
    SkinningInformationForCompute* computeSkinningInfoData_ = nullptr;
    D3D12_VERTEX_BUFFER_VIEW computeOutputVertexBufferView_{};
    D3D12_RESOURCE_STATES computeOutputVertexState_ = D3D12_RESOURCE_STATE_COMMON;
    uint32_t computePaletteSrvIndex_ = UINT32_MAX;
    uint32_t computeInputVertexSrvIndex_ = UINT32_MAX;
    uint32_t computeInfluenceSrvIndex_ = UINT32_MAX;
    uint32_t computeOutputVertexUavIndex_ = UINT32_MAX;
    bool enableComputeSkinning_ = false;
    Skeleton skeleton_{};
    std::vector<Matrix4x4> inverseBindPoseMatrices_;
    bool hasSkeleton_ = false;
    float animationTime_ = 0.0f;
};
