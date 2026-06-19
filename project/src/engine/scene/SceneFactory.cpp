#include "engine/scene/SceneFactory.h"
#include "engine/scene/TitleScene.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/EditorScene.h"

std::unique_ptr<BaseScene> SceneFactory::CreateScene(SceneType sceneType) {
    switch (sceneType) {
    case SceneType::Title:
        return std::make_unique<TitleScene>();

    case SceneType::Game:
        return std::make_unique<GameScene>();

    case SceneType::Editor:
        return std::make_unique<EditorScene>();

    default:
        return nullptr;
    }
}
