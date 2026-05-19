#include "engine/scene/SceneFactory.h"
#include "engine/scene/TitleScene.h"
#include "engine/scene/GamePlayScene.h"
#include "app/GameScene.h"
#include "app/ResultScene.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(SceneType sceneType) {
    switch (sceneType) {
    case SceneType::Title:
        return std::make_unique<TitleScene>();

    case SceneType::GamePlay:
        return std::make_unique<GamePlayScene>();

    case SceneType::Game:
        return std::make_unique<GameScene>();

    case SceneType::Result:
        return std::make_unique<ResultScene>();

    default:
        return nullptr;
    }
}
