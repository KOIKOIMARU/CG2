#include "engine/scene/GameScene.h"

#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

void GameScene::Initialize()
{
    runtime_.SetSystems(
        dxCommon_,
        srvManager_,
        spriteCommon_,
        imguiManager_,
        input_);
    runtime_.Initialize();
}

void GameScene::Finalize()
{
    runtime_.Finalize();
}

void GameScene::Update()
{
    runtime_.Update();
    if (runtime_.IsExitRequested() && sceneManager_) {
        sceneManager_->SetNextScene(SceneType::Title);
    }
}

void GameScene::Draw()
{
    runtime_.Draw();
}

int GameScene::GetPostEffectMode() const
{
    return runtime_.GetPostEffectMode();
}
