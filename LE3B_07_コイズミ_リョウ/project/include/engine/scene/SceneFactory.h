#pragma once
#include "engine/scene/AbstractSceneFactory.h"

class SceneFactory : public AbstractSceneFactory {
public:
    ~SceneFactory() override = default;
    std::unique_ptr<BaseScene> CreateScene(SceneType sceneType) override;
};