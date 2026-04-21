#include <vector>
#include <fstream>
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

    transform_ = {
        {1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 0.0f},
        {0.0f, 0.0f, 0.0f}
    };

}


void Object3d::Update() {
    // ワールド行列
    Matrix4x4 worldMatrix = MakeAffineMatrix(
        transform_.scale,
        transform_.rotate,
        transform_.translate
    );

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

// ===== setter =====
void Object3d::SetScale(const Vector3& scale) {
    transform_.scale = scale;
}

void Object3d::SetRotate(const Vector3& rotate) {
    transform_.rotate = rotate;
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
}
