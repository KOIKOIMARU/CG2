#pragma once
#include "engine/scene/BaseScene.h"
#include "engine/scene/SceneType.h"
#include <memory>
#include <vector>
#include "engine/base/Math.h"
#include "engine/2d/Sprite.h"

class TitleScene : public BaseScene {
public:
    TitleScene();
    ~TitleScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
    void RequestStart(bool tutorial = false);

private:
    std::vector<Sprite> sprites_;
    Math::Vector2 spritePos_{ 200.0f, 120.0f };
    bool startRequested_ = false;
    SceneType requestedScene_ = SceneType::Game;
};
