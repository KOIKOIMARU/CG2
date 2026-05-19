#pragma once

#include "engine/scene/BaseScene.h"

class ResultScene : public BaseScene {
public:
    ResultScene();
    ~ResultScene() override;

    void Initialize() override;
    void Finalize() override;
    void Update() override;
    void Draw() override;
};
