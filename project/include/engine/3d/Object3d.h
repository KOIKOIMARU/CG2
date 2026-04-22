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

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
    Matrix4x4 WorldInverseTranspose;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
    Vector3 padding;
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

class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();
    void UpdateAnimation(float deltaTime);

    void SetModel(Model* model);

    // setter
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
    void SetAlphaReference(float alphaReference);
    void SetUVTransform(const Matrix4x4& uvTransform);
    void SetLightingMode(int32_t lightingMode);
    bool HasSkeleton() const { return hasSkeleton_; }
    const Skeleton& GetSkeleton() const { return skeleton_; }
    const Matrix4x4& GetWorldMatrix() const { return worldMatrix_; }

    // getter
    Vector3 GetScale() const;
    Vector3 GetRotate() const;
    Vector3 GetTranslate() const;
    float GetEnvironmentCoefficient() const;

    void SetModel(const std::string& filePath);

    void SetCamera(Camera* camera) { this->camera_ = camera; }


private:
    void CreateTransformationMatrix();
    void CreateDirectionalLight();
    void CreateCameraResource();
    void CreatePointLight();
    void CreateSpotLight();
    void CreateSkinningPalette();
    void InitializeSkinning();
    void UpdateSkinningPalette();

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

    ComPtr<ID3D12Resource> cameraResource_;
    CameraForGPU* cameraData_ = nullptr;

    ComPtr<ID3D12Resource> pointLightResource_;
    PointLight* pointLightData_ = nullptr;

    ComPtr<ID3D12Resource> spotLightResource_;
    SpotLight* spotLightData_ = nullptr;

    ComPtr<ID3D12Resource> skinningPaletteResource_;
    SkinningPaletteForGPU* skinningPaletteData_ = nullptr;
    Skeleton skeleton_{};
    std::vector<Matrix4x4> inverseBindPoseMatrices_;
    bool hasSkeleton_ = false;
    float animationTime_ = 0.0f;
};
