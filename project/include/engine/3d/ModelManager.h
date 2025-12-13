#pragma once
#include <map>
#include <memory>
#include <string>

class Model;
class ModelCommon;
class DirectXCommon;

class ModelManager {
public:
    static ModelManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon);
    void Finalize();

    // モデル読み込み
    void LoadModel(const std::string& filePath);

    // モデル取得
    Model* FindModel(const std::string& filePath);

private:
    ModelManager() = default;
    ~ModelManager() = default;
    ModelManager(const ModelManager&) = delete;
    ModelManager& operator=(const ModelManager&) = delete;

private:
    static ModelManager* instance;

    std::map<std::string, std::unique_ptr<Model>> models_;
    ModelCommon* modelCommon_ = nullptr;
};
