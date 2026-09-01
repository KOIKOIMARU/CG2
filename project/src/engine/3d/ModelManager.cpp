#include "engine/3d/ModelManager.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelCommon.h"
#include "engine/base/DirectXCommon.h"
#include <stdexcept>

ModelManager* ModelManager::GetInstance()
{
    static ModelManager instance;
    return &instance;
}

void ModelManager::Initialize(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    if (modelCommon_) {
        return;
    }

    modelCommon_ = std::make_unique<ModelCommon>();
    modelCommon_->Initialize(dxCommon, srvManager);
}

void ModelManager::Finalize()
{
    models_.clear();

    modelCommon_.reset();
}

void ModelManager::LoadModel(const std::string& filePath)
{
    // すでに読み込み済みなら何もしない
    if (models_.contains(filePath)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(GetInitializedModelCommon(), "resources", filePath);

    models_.insert(std::make_pair(filePath, std::move(model)));
}

void ModelManager::CreateTriangle(
    const std::string& name,
    float width,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateTriangleData(width, height, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreatePlane(
    const std::string& name,
    float width,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreatePlaneData(width, height, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateCircle(
    const std::string& name,
    uint32_t divideCount,
    float radius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateCircleData(divideCount, radius, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateHeart(
    const std::string& name,
    uint32_t divideCount,
    float radius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateHeartData(divideCount, radius, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateRing(
    const std::string& name,
    uint32_t divideCount,
    float outerRadius,
    float innerRadius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateRingData(
            divideCount,
            outerRadius,
            innerRadius,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateSphere(
    const std::string& name,
    uint32_t latDivideCount,
    uint32_t lonDivideCount,
    float radius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateSphereData(
            latDivideCount,
            lonDivideCount,
            radius,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateTorus(
    const std::string& name,
    uint32_t majorDivideCount,
    uint32_t minorDivideCount,
    float majorRadius,
    float minorRadius,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateTorusData(
            majorDivideCount,
            minorDivideCount,
            majorRadius,
            minorRadius,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateCylinder(
    const std::string& name,
    uint32_t divideCount,
    float topRadius,
    float bottomRadius,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateCylinderData(
            divideCount,
            topRadius,
            bottomRadius,
            height,
            textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateCone(
    const std::string& name,
    uint32_t divideCount,
    float radius,
    float height,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateConeData(divideCount, radius, height, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::CreateBox(
    const std::string& name,
    float width,
    float height,
    float depth,
    const std::string& textureFilePath)
{
    if (models_.contains(name)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(
        GetInitializedModelCommon(),
        Model::CreateBoxData(width, height, depth, textureFilePath)
    );

    models_.insert(std::make_pair(name, std::move(model)));
}

void ModelManager::SetEnvironmentTexturePath(const std::string& texturePath)
{
    if (modelCommon_) {
        modelCommon_->SetEnvironmentTexturePath(texturePath);
    }
}

Model* ModelManager::FindModel(const std::string& filePath)
{
    if (models_.contains(filePath)) {
        return models_.at(filePath).get();
    }
    return nullptr;
}

ModelCommon* ModelManager::GetInitializedModelCommon() const
{
    if (!modelCommon_) {
        throw std::logic_error("ModelManager must be initialized before creating models");
    }
    return modelCommon_.get();
}
