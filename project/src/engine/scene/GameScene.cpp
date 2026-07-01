#include "engine/scene/GameScene.h"

#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

GameScene::GameScene() = default;
GameScene::~GameScene() = default;

bool GameScene::PreloadResourcesStep(DirectXCommon* dxCommon, SrvManager* srvManager)
{
    return GameRuntime::PreloadSharedResourceStep(dxCommon, srvManager);
}

bool GameScene::AreResourcesPreloaded()
{
    return GameRuntime::AreSharedResourcesPreloaded();
}

int GameScene::GetResourcePreloadStep()
{
    return GameRuntime::GetSharedResourcePreloadStep();
}

int GameScene::GetResourcePreloadStepCount()
{
    return GameRuntime::GetSharedResourcePreloadStepCount();
}

const char* GameScene::GetResourcePreloadLabel()
{
    return GameRuntime::GetSharedResourcePreloadLabel();
}

float GameScene::GetResourcePreloadLastStepMs()
{
    return GameRuntime::GetSharedResourceLastStepMs();
}

float GameScene::GetResourcePreloadTotalMs()
{
    return GameRuntime::GetSharedResourceTotalMs();
}

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

const Math::Matrix4x4& GameScene::GetProjectionMatrix() const
{
    return runtime_.GetProjectionMatrix();
}
