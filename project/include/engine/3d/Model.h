#pragma once
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>

#include "engine/base/Math.h"

using Microsoft::WRL::ComPtr;
using namespace Math;

// ===========================
//  モデル用データ構造
// ===========================
struct VertexData {
    Vector4 position;
    Vector2 texcoord;
    Vector3 normal;
};

struct MaterialData {
    std::string textureFilePath;
};

struct ModelData {
    std::vector<VertexData> vertices;
    MaterialData material;
};

struct Material {
    Vector4 color;
    int32_t lightingMode;
    float shininess;
    float environmentCoefficient;
    float alphaReference;
    Vector3 specularColor;
    float padding1;
    Matrix4x4 uvTransform;
};

class ModelCommon;

class Model {
public:
    void Initialize(ModelCommon* modelCommon, const std::string& directoryPath, const std::string& filename);
    void Initialize(ModelCommon* modelCommon, const ModelData& modelData);
    void Draw();
    void SetEnvironmentCoefficient(float coefficient);
    float GetEnvironmentCoefficient() const;
    void SetColor(const Vector4& color);
    void SetAlphaReference(float alphaReference);
    void SetUVTransform(const Matrix4x4& uvTransform);
    void SetLightingMode(int32_t lightingMode);

    // Object3d が必要になったら使う用（次段階用）
    const D3D12_VERTEX_BUFFER_VIEW& GetVBV() const { return vertexBufferView_; }
    ID3D12Resource* GetMaterialResource() const { return materialResource_.Get(); }
    size_t GetVertexCount() const { return modelData_.vertices.size(); }

    static MaterialData LoadMaterialTemplate(const std::string& directoryPath, const std::string& filename);
    static ModelData LoadObjFile(const std::string& directoryPath, const std::string& filename);
    static ModelData LoadAssimpFile(const std::string& directoryPath, const std::string& filename);
    static ModelData CreateTriangleData(float width, float height, const std::string& textureFilePath);
    static ModelData CreatePlaneData(float width, float height, const std::string& textureFilePath);
    static ModelData CreateCircleData(
        uint32_t divideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateRingData(
        uint32_t divideCount,
        float outerRadius,
        float innerRadius,
        const std::string& textureFilePath);
    static ModelData CreateSphereData(
        uint32_t latDivideCount,
        uint32_t lonDivideCount,
        float radius,
        const std::string& textureFilePath);
    static ModelData CreateTorusData(
        uint32_t majorDivideCount,
        uint32_t minorDivideCount,
        float majorRadius,
        float minorRadius,
        const std::string& textureFilePath);
    static ModelData CreateCylinderData(
        uint32_t divideCount,
        float topRadius,
        float bottomRadius,
        float height,
        const std::string& textureFilePath);
    static ModelData CreateConeData(
        uint32_t divideCount,
        float radius,
        float height,
        const std::string& textureFilePath);
    static ModelData CreateBoxData(
        float width,
        float height,
        float depth,
        const std::string& textureFilePath);

private:
    void CreateVertexBuffer();
    void CreateMaterial();

private:
    ModelCommon* modelCommon_ = nullptr;

    ModelData modelData_;

    ComPtr<ID3D12Resource> vertexResource_;
    D3D12_VERTEX_BUFFER_VIEW vertexBufferView_{};

    ComPtr<ID3D12Resource> materialResource_;
    Material* materialData_ = nullptr;
};
