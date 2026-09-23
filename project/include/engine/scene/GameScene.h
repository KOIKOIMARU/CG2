#pragma once

#include "app/GameRuntime.h"
#include "engine/scene/BaseScene.h"

class GameScene : public BaseScene {
public:
    explicit GameScene(GameRuntime::PlayMode mode = GameRuntime::PlayMode::Game);
    ~GameScene() override;

    static bool PreloadResourcesStep(DirectXCommon* dxCommon, SrvManager* srvManager);
    static bool AreResourcesPreloaded();
    static int GetResourcePreloadStep();
    static int GetResourcePreloadStepCount();
    static const char* GetResourcePreloadLabel();
    static float GetResourcePreloadLastStepMs();
    static float GetResourcePreloadTotalMs();

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

    int GetPostEffectMode() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;
    bool IsTutorial() const { return mode_ == GameRuntime::PlayMode::Tutorial; }

private:
    GameRuntime::PlayMode mode_; // SceneFactoryで指定する入場先。途中では切り替えない。
    GameRuntime runtime_;
};
