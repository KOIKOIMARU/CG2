#include "app/AppSceneFactory.h"
#include "app/TitleScene.h"
#include "app/GameScene.h"
#include "samples/rail_shooter/GameRuntime.h"
#include "engine/scene/EditorScene.h"

// app層だけが具体シーンとサンプルランタイムを知り、engine層へは抽象型を返す。
std::unique_ptr<BaseScene> AppSceneFactory::CreateScene(SceneType sceneType) {
    switch (sceneType) {
    case SceneType::Title:
        return std::make_unique<TitleScene>();

    case SceneType::Game:
        return std::make_unique<GameScene>();

    case SceneType::Editor:
    {
        auto editorScene = std::make_unique<EditorScene>();
        editorScene->SetPlayRuntimeFactory([] {
            return std::make_unique<GameRuntime>();
        });
        return editorScene;
    }

    default:
        return nullptr;
    }
}
