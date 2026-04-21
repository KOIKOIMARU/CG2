#pragma once
#include <map>
#include <memory>
#include <string>

class Model;
class ModelCommon;
class DirectXCommon;
class SrvManager;

class ModelManager {
public:
    static ModelManager* GetInstance();

    void Initialize(DirectXCommon* dxCommon, SrvManager* srvManager);
    void Finalize();

    // モデル読み込み
    void LoadModel(const std::string& filePath);
    void CreatePlane(
        const std::string& name,
        float width,
        float height,
        const std::string& textureFilePath);
    void CreateRing(
        const std::string& name,
        uint32_t divideCount,
        float outerRadius,
        float innerRadius,
        const std::string& textureFilePath);
    void SetEnvironmentTexturePath(const std::string& texturePath);

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
