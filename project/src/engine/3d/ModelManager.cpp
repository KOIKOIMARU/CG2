#include "engine/3d/ModelManager.h"
#include "engine/3d/Model.h"
#include "engine/3d/ModelCommon.h"
#include "engine/base/DirectXCommon.h"

ModelManager* ModelManager::instance = nullptr;

ModelManager* ModelManager::GetInstance()
{
    if (!instance) {
        instance = new ModelManager();
    }
    return instance;
}

void ModelManager::Initialize(DirectXCommon* dxCommon)
{
    modelCommon_ = new ModelCommon();
    modelCommon_->Initialize(dxCommon);
}

void ModelManager::Finalize()
{
    models_.clear();

    delete modelCommon_;
    modelCommon_ = nullptr;

    delete instance;
    instance = nullptr;
}

void ModelManager::LoadModel(const std::string& filePath)
{
    // すでに読み込み済みなら何もしない
    if (models_.contains(filePath)) {
        return;
    }

    auto model = std::make_unique<Model>();
    model->Initialize(modelCommon_, "resources", filePath);

    models_.insert(std::make_pair(filePath, std::move(model)));
}

Model* ModelManager::FindModel(const std::string& filePath)
{
    if (models_.contains(filePath)) {
        return models_.at(filePath).get();
    }
    return nullptr;
}
