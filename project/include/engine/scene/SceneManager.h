#pragma once
#include <memory>
#include "engine/scene/SceneType.h"

class BaseScene;
class AbstractSceneFactory;
class DirectXCommon;
class SrvManager;
class SpriteCommon;
class ImGuiManager;
class Input;

// シーンの生成、初期化、更新、破棄と遷移を一元管理するシングルトン。
// SetNextScene は次回 Update の先頭で安全に切り替えるための予約である。
class SceneManager {
public:
    static SceneManager* GetInstance();

    SceneManager(const SceneManager&) = delete;
    SceneManager& operator=(const SceneManager&) = delete;

    ~SceneManager();

    void Update();
    void Draw();
    BaseScene* GetCurrentScene() const { return scene_.get(); }
    void FinalizeCurrentScene();

    void SetNextScene(SceneType sceneType);
    // 重い初期化を遷移前に済ませたい場合の事前生成。現在シーンは維持される。
    void PrepareScene(SceneType sceneType);
    bool IsScenePrepared(SceneType sceneType) const;
    void SetSceneFactory(AbstractSceneFactory* sceneFactory);

    void SetSystems(
        DirectXCommon* dxCommon,
        SrvManager* srvManager,
        SpriteCommon* spriteCommon,
        ImGuiManager* imguiManager,
        Input* input
    );

private:
    SceneManager() = default;

private:
    std::unique_ptr<BaseScene> scene_;
    std::unique_ptr<BaseScene> preparedScene_;
    SceneType preparedSceneType_{};
    bool hasPreparedScene_ = false;
    SceneType nextSceneType_{};
    bool hasNextScene_ = false;

    AbstractSceneFactory* sceneFactory_ = nullptr;

    DirectXCommon* dxCommon_ = nullptr;
    SrvManager* srvManager_ = nullptr;
    SpriteCommon* spriteCommon_ = nullptr;
    ImGuiManager* imguiManager_ = nullptr;
    Input* input_ = nullptr;
};
