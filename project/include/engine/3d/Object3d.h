#pragma once
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

class Object3dCommon;
class Model;

struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
    Vector3 padding;
};

class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();

    void SetModel(Model* model) { model_ = model; }

    // setter
    void SetScale(const Vector3& scale);
    void SetRotate(const Vector3& rotate);
    void SetTranslate(const Vector3& translate);

    // getter
    Vector3 GetScale() const;
    Vector3 GetRotate() const;
    Vector3 GetTranslate() const;


private:
    void CreateTransformationMatrix();
    void CreateDirectionalLight();

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model* model_ = nullptr;

    Transform transform_;
    Transform cameraTransform_;

    ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;
};
