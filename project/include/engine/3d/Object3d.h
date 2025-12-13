#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct MaterialData {
    std::string textureFilePath;
    uint32_t textureIndex = 0; // ★追加
};

struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};

struct Material {
    Vector4 color;
    int32_t lightingMode;
    float padding[3];
    Matrix4x4 uvTransform;
};


struct TransformationMatrix {
    Matrix4x4 WVP;
    Matrix4x4 World;
};

struct DirectionalLight {
    Vector4 color;
    Vector3 direction;
    float intensity;
    Vector3 padding; // ← float3 paddingで16バイト境界に揃える
};



class Object3dCommon;

class Object3d {
public:
    void Initialize(Object3dCommon* object3dCommon);
    void Update();
    void Draw();

    static MaterialData LoadMaterialTemplate(
        const std::string& directoryPath,
        const std::string& filename);

    static ModelData LoadObjFile(
        const std::string& directoryPath,
        const std::string& filename);

private:
    void CreateVertexBuffer(); // ★資料にあるやつ

    void CreateMaterial();

    void CreateTransformationMatrix();

    void CreateDirectionalLight();

private:
    Object3dCommon* object3dCommon_ = nullptr;

    ModelData modelData_;

    ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;

    ComPtr<ID3D12Resource> transformationMatrixResource_;
    TransformationMatrix* transformationMatrixData_ = nullptr;

    ComPtr<ID3D12Resource> directionalLightResource_;
    DirectionalLight* directionalLightData_ = nullptr;

    Transform transform_;
    Transform cameraTransform_;
};
