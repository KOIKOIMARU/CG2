#include <vector>
#include <fstream>
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
}



void Object3d::Draw()
{
    auto commandList = object3dCommon_->GetDxCommon()->GetCommandList();

    // Transform
    commandList->SetGraphicsRootConstantBufferView(
        1, transformationMatrixResource_->GetGPUVirtualAddress());

    // Light
    commandList->SetGraphicsRootConstantBufferView(
        3, directionalLightResource_->GetGPUVirtualAddress());

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

void Object3d::SetModel(const std::string& filePath)
{
    model_ = ModelManager::GetInstance()->FindModel(filePath);
}
