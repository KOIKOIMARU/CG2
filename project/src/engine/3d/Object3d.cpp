#include <vector>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <d3d12.h>
#include <wrl/client.h>

#include "engine/base/Math.h"
#include "engine/base/DirectXCommon.h"
#include "engine/3d/Object3dCommon.h"
#include "engine/3d/Object3d.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelManager.h"
#include "engine/3d/Camera.h"



void Object3d::Initialize(Object3dCommon* object3dCommon)
{
    assert(object3dCommon);
    object3dCommon_ = object3dCommon;

    camera_ = object3dCommon_->GetDefaultCamera();

    CreateTransformationMatrix();
    CreateDirectionalLight();
    CreateCameraResource();
    CreatePointLight();
    CreateSpotLight();
    CreateSkinningPalette();

    transform_ = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

}


void Object3d::Update() {
    // ワールド行列
    Matrix4x4 worldMatrix;
    if (useQuaternionRotate_) {
        worldMatrix = MakeAffineMatrix(
            transform_.scale,
            quaternionRotate_,
            transform_.translate
        );
    } else {
        worldMatrix = MakeAffineMatrix(
            transform_.scale,
            transform_.rotate,
            transform_.translate
        );
    }
    worldMatrix_ = worldMatrix;

    Matrix4x4 worldViewProjectionMatrix;

    if (camera_) {
        const Matrix4x4& viewProjection =
            camera_->GetViewProjectionMatrix();
        worldViewProjectionMatrix =
            Multiply(worldMatrix, viewProjection);
    } else {
        // カメラが無くても一応描画可能
        worldViewProjectionMatrix = worldMatrix;
    }

    transformationMatrixData_->WVP =
        Transpose(worldViewProjectionMatrix);
    transformationMatrixData_->World =
        Transpose(worldMatrix);
    transformationMatrixData_->WorldInverseTranspose =
        Transpose(Inverse(worldMatrix));

    if (camera_) {
        cameraData_->worldPosition = camera_->GetTranslate();
    }
}

void Object3d::UpdateAnimation(float deltaTime)
{
    if (!model_ || !hasSkeleton_) {
        if (skinningPaletteData_) {
            skinningPaletteData_->enableSkinning = 0;
        }
        return;
    }

    const Animation& animation = model_->GetAnimation();
    if (animation.duration > 0.0f && model_->HasAnimation()) {
        animationTime_ += deltaTime * animation.ticksPerSecond;
        while (animationTime_ > animation.duration) {
            animationTime_ -= animation.duration;
        }
        Model::ApplyAnimation(skeleton_, animation, animationTime_);
    } else {
        for (Joint& joint : skeleton_.joints) {
            joint.transform = joint.bindPoseTransform;
        }
    }

    Model::UpdateSkeleton(skeleton_);
    UpdateSkinningPalette();
}



void Object3d::Draw()
{
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // Transform
    commandList->SetGraphicsRootConstantBufferView(
        1, transformationMatrixResource_->GetGPUVirtualAddress());

    // Light
    commandList->SetGraphicsRootConstantBufferView(
        2, cameraResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        5, directionalLightResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        6, pointLightResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        7, spotLightResource_->GetGPUVirtualAddress());

    commandList->SetGraphicsRootConstantBufferView(
        8, skinningPaletteResource_->GetGPUVirtualAddress());

    // Model 描画
    if (model_) {
        model_->Draw();
    }
}

void Object3d::CreateTransformationMatrix() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    // 座標変換行列用リソースを作成
    transformationMatrixResource_ =
        dxCommon->CreateBufferResource(sizeof(TransformationMatrix));

    // Map してポインタ取得
    transformationMatrixResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&transformationMatrixData_));

    // 単位行列で初期化
    transformationMatrixData_->WVP = MakeIdentity4x4();
    transformationMatrixData_->World = MakeIdentity4x4();
    transformationMatrixData_->WorldInverseTranspose = MakeIdentity4x4();
}

void Object3d::CreateDirectionalLight() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    // 平行光源用 ConstantBuffer を作成
    directionalLightResource_ =
        dxCommon->CreateBufferResource(sizeof(DirectionalLight));

    // Map
    directionalLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&directionalLightData_));

    // 初期化（資料準拠）
    directionalLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    directionalLightData_->direction = { 0.0f, -1.0f, 0.0f };
    directionalLightData_->intensity = 1.0f;
}

void Object3d::CreateCameraResource() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    cameraResource_ =
        dxCommon->CreateBufferResource(sizeof(CameraForGPU));

    cameraResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&cameraData_));

    cameraData_->worldPosition = { 0.0f, 0.0f, -5.0f };
    cameraData_->padding = 0.0f;
}

void Object3d::CreatePointLight() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    pointLightResource_ =
        dxCommon->CreateBufferResource(sizeof(PointLight));

    pointLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&pointLightData_));

    pointLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    pointLightData_->position = { 0.0f, 2.0f, 0.0f };
    pointLightData_->intensity = 1.0f;
    pointLightData_->radius = 6.0f;
    pointLightData_->decay = 2.0f;
}

void Object3d::CreateSpotLight() {
    auto dxCommon = object3dCommon_->GetDxCommon();

    spotLightResource_ =
        dxCommon->CreateBufferResource(sizeof(SpotLight));

    spotLightResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&spotLightData_));

    spotLightData_->color = { 1.0f, 1.0f, 1.0f, 1.0f };
    spotLightData_->position = { 2.0f, 1.25f, 0.0f };
    spotLightData_->direction = Normalize({ -1.0f, 1.0f, 0.0f });
    spotLightData_->intensity = 4.0f;
    spotLightData_->distance = 7.0f;
    spotLightData_->decay = 2.0f;
    spotLightData_->cosAngle = std::cos(3.14159265f / 3.0f);
    spotLightData_->cosFalloffStart = std::cos(3.14159265f / 6.0f);
}

void Object3d::CreateSkinningPalette()
{
    auto dxCommon = object3dCommon_->GetDxCommon();

    skinningPaletteResource_ =
        dxCommon->CreateBufferResource(sizeof(SkinningPaletteForGPU));

    skinningPaletteResource_->Map(
        0, nullptr,
        reinterpret_cast<void**>(&skinningPaletteData_));

    skinningPaletteData_->enableSkinning = 0;
    for (uint32_t jointIndex = 0; jointIndex < kNumMaxSkeletonJoints; ++jointIndex) {
        skinningPaletteData_->palette[jointIndex].skeletonSpaceMatrix =
            MakeIdentity4x4();
        skinningPaletteData_->palette[jointIndex].skeletonSpaceInverseTransposeMatrix =
            MakeIdentity4x4();
    }
}

void Object3d::InitializeSkinning()
{
    hasSkeleton_ = false;
    inverseBindPoseMatrices_.clear();
    animationTime_ = 0.0f;

    if (!model_ || !model_->HasSkinCluster()) {
        if (skinningPaletteData_) {
            skinningPaletteData_->enableSkinning = 0;
        }
        return;
    }

    skeleton_ = Model::CreateSkeleton(model_->GetRootNode());
    inverseBindPoseMatrices_.assign(
        skeleton_.joints.size(),
        MakeIdentity4x4()
    );

    const SkinClusterData& skinClusterData = model_->GetSkinClusterData();
    for (const auto& [jointName, jointWeightData] : skinClusterData.jointWeights) {
        auto jointIt = skeleton_.jointMap.find(jointName);
        if (jointIt == skeleton_.jointMap.end()) {
            continue;
        }

        inverseBindPoseMatrices_[jointIt->second] =
            jointWeightData.inverseBindPoseMatrix;
    }

    Model::UpdateSkeleton(skeleton_);
    UpdateSkinningPalette();
    hasSkeleton_ = true;
}

void Object3d::UpdateSkinningPalette()
{
    if (!skinningPaletteData_) {
        return;
    }

    if (!hasSkeleton_) {
        skinningPaletteData_->enableSkinning = 0;
        return;
    }

    skinningPaletteData_->enableSkinning = 1;
    const size_t jointCount =
        std::min<size_t>(skeleton_.joints.size(), kNumMaxSkeletonJoints);

    for (size_t jointIndex = 0; jointIndex < jointCount; ++jointIndex) {
        const Matrix4x4 skinningMatrix = Multiply(
            inverseBindPoseMatrices_[jointIndex],
            skeleton_.joints[jointIndex].skeletonSpaceMatrix
        );

        skinningPaletteData_->palette[jointIndex].skeletonSpaceMatrix =
            Transpose(skinningMatrix);
        skinningPaletteData_->palette[jointIndex].skeletonSpaceInverseTransposeMatrix =
            Transpose(Inverse(skinningMatrix));
    }

    for (size_t jointIndex = jointCount; jointIndex < kNumMaxSkeletonJoints; ++jointIndex) {
        skinningPaletteData_->palette[jointIndex].skeletonSpaceMatrix =
            MakeIdentity4x4();
        skinningPaletteData_->palette[jointIndex].skeletonSpaceInverseTransposeMatrix =
            MakeIdentity4x4();
    }
}

// ===== setter =====
void Object3d::SetScale(const Vector3& scale) {
    transform_.scale = scale;
}

void Object3d::SetRotate(const Vector3& rotate) {
    transform_.rotate = rotate;
    useQuaternionRotate_ = false;
}

void Object3d::SetQuaternionRotate(const Quaternion& rotate)
{
    quaternionRotate_ = NormalizeQuaternion(rotate);
    useQuaternionRotate_ = true;
}

void Object3d::SetTranslate(const Vector3& translate) {
    transform_.translate = translate;
}

void Object3d::SetDirectionalLightDirection(const Vector3& direction) {
    directionalLightData_->direction = direction;
}

void Object3d::SetDirectionalLightIntensity(float intensity) {
    directionalLightData_->intensity = intensity;
}

void Object3d::SetPointLightPosition(const Vector3& position) {
    pointLightData_->position = position;
}

void Object3d::SetPointLightIntensity(float intensity) {
    pointLightData_->intensity = intensity;
}

void Object3d::SetSpotLightPosition(const Vector3& position) {
    spotLightData_->position = position;
}

void Object3d::SetSpotLightDirection(const Vector3& direction) {
    spotLightData_->direction = Normalize(direction);
}

void Object3d::SetSpotLightIntensity(float intensity) {
    spotLightData_->intensity = intensity;
}

void Object3d::SetModel(Model* model)
{
    model_ = model;
    InitializeSkinning();
}

void Object3d::SetEnvironmentCoefficient(float coefficient)
{
    if (model_) {
        model_->SetEnvironmentCoefficient(coefficient);
    }
}

void Object3d::SetColor(const Vector4& color)
{
    if (model_) {
        model_->SetColor(color);
    }
}

void Object3d::SetAlphaReference(float alphaReference)
{
    if (model_) {
        model_->SetAlphaReference(alphaReference);
    }
}

void Object3d::SetUVTransform(const Matrix4x4& uvTransform)
{
    if (model_) {
        model_->SetUVTransform(uvTransform);
    }
}

void Object3d::SetLightingMode(int32_t lightingMode)
{
    if (model_) {
        model_->SetLightingMode(lightingMode);
    }
}

// ===== getter =====
Vector3 Object3d::GetScale() const {
    return transform_.scale;
}

Vector3 Object3d::GetRotate() const {
    return transform_.rotate;
}

Vector3 Object3d::GetTranslate() const {
    return transform_.translate;
}

float Object3d::GetEnvironmentCoefficient() const
{
    if (model_) {
        return model_->GetEnvironmentCoefficient();
    }
    return 0.0f;
}

void Object3d::SetModel(const std::string& filePath)
{
    model_ = ModelManager::GetInstance()->FindModel(filePath);
    InitializeSkinning();
}
