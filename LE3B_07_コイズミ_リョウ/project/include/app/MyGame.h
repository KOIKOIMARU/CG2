#pragma once
#include "engine/base/Framework.h"
#include <memory>

class AbstractSceneFactory;

class MyGame : public Framework {
public:
    MyGame();
    ~MyGame() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;

private:
    std::unique_ptr<AbstractSceneFactory> sceneFactory_;
};