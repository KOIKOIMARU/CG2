#pragma once

#include "app/GameRuntime.h"
#include "engine/scene/BaseScene.h"

class GameScene : public BaseScene {
public:
    GameScene();
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

private:
    GameRuntime runtime_;
};
