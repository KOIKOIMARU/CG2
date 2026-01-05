#pragma once
#include <wrl.h>
#include <d3d12.h>
#include <string>
#include <cstdint>
#include "engine/base/Math.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

class Object3dCommon;
class Model;
class Camera;

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

// ★追加：PixelShader(b0)の MaterialCB と一致させる
struct ObjectMaterial {
    Vector4 color;
    int32_t enableLighting;
    float padding[3];
    Matrix4x4 uvTransform;
};


class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();

    void SetModel(Model* model) { model_ = model; }
    void SetModel(const std::string& filePath);

    // setter
    void SetScale(const Vector3& scale);
    void SetRotate(const Vector3& rotate);
    void SetTranslate(const Vector3& translate);

    // getter
    Vector3 GetScale() const;
    Vector3 GetRotate() const;
    Vector3 GetTranslate() const;

    void SetCamera(Camera* camera) { camera_ = camera; }

    // ★追加：影(陰影)のON/OFF
    void SetEnableLighting(int32_t mode); // 0/1/2
    void SetColor(const Vector4& color);

private:
    void CreateTransformationMatrix();
    void CreateDirectionalLight();
    void CreateMaterial(); // ★追加

private:
    Object3dCommon* object3dCommon_ = nullptr;
    Model* model_ = nullptr;

    Transform transform_;
    Camera* camera_ = nullptr;

    ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;

    // ★追加：MaterialCB(b0)
    ComPtr<ID3D12Resource> materialResource_;
    ObjectMaterial* materialData_ = nullptr;

};
