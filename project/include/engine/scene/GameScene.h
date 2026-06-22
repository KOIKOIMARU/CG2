#pragma once

#include "app/GameRuntime.h"
#include "engine/scene/BaseScene.h"

class GameScene : public BaseScene {
public:
    GameScene();
    ~GameScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

    int GetPostEffectMode() const;
    const Math::Matrix4x4& GetProjectionMatrix() const;

private:
    GameRuntime runtime_;
};
