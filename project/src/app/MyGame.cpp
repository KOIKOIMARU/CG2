#include "app/MyGame.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneFactory.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/base/ImGuiManager.h"
#include "engine/3d/ModelManager.h"
#include "engine/io/Input.h"
#include "engine/scene/EditorScene.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneType.h"

#include <chrono>

namespace {

float ToMilliseconds(
    const std::chrono::steady_clock::time_point& begin,
    const std::chrono::steady_clock::time_point& end)
{
    return std::chrono::duration<float, std::milli>(end - begin).count();
}

}

MyGame::MyGame() = default;
MyGame::~MyGame() = default;

void MyGame::Initialize() {
    Framework::Initialize();

    sceneFactory_ = std::make_unique<SceneFactory>();

    SceneManager::GetInstance()->SetSceneFactory(sceneFactory_.get());
    SceneManager::GetInstance()->SetSystems(
        dxCommon_.get(),
        srvManager_.get(),
        spriteCommon_.get(),
        imguiManager_.get(),
        input_.get()
    );

    SceneManager::GetInstance()->SetNextScene(SceneType::Title);
}

void MyGame::Update() {
    const auto updateBegin = std::chrono::steady_clock::now();
    Framework::Update();
    if (endRequst_) {
        if (dxCommon_) {
            dxCommon_->EditFrameTiming().updateMs =
                ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
        }
        return;
    }

    imguiManager_->Begin();

    SceneManager::GetInstance()->Update();

    imguiManager_->End();

    dxCommon_->EditFrameTiming().updateMs =
        ToMilliseconds(updateBegin, std::chrono::steady_clock::now());
}

void MyGame::Draw() {
    auto& timing = dxCommon_->EditFrameTiming();
    const auto drawBegin = std::chrono::steady_clock::now();
    auto sectionBegin = drawBegin;

    dxCommon_->PreDraw();
    srvManager_->PreDraw();
    timing.preDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    SceneManager::GetInstance()->Draw();
    timing.sceneDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    int postEffectMode = 0;
    if (auto* editorScene =
            dynamic_cast<EditorScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        postEffectMode = editorScene->GetPostEffectMode();
        dxCommon_->SetPostEffectProjectionMatrix(editorScene->GetProjectionMatrix());
    } else if (auto* gameScene =
            dynamic_cast<GameScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        postEffectMode = gameScene->GetPostEffectMode();
        dxCommon_->SetPostEffectProjectionMatrix(gameScene->GetProjectionMatrix());
    }

    sectionBegin = std::chrono::steady_clock::now();
    dxCommon_->DrawRenderTextureToSwapChain(postEffectMode);
    timing.postEffectMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    sectionBegin = std::chrono::steady_clock::now();
    imguiManager_->Draw();
    timing.imguiDrawMs =
        ToMilliseconds(sectionBegin, std::chrono::steady_clock::now());

    dxCommon_->PostDraw();
    timing.frameCpuMs =
        timing.updateMs +
        ToMilliseconds(drawBegin, std::chrono::steady_clock::now());
}

void MyGame::Finalize() {
    SceneManager::GetInstance()->FinalizeCurrentScene();
    ModelManager::GetInstance()->Finalize();
    sceneFactory_.reset();
    Framework::Finalize();
}
