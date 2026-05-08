#include "app/MyGame.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneFactory.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/base/ImGuiManager.h"
#include "engine/io/Input.h"
#include "engine/scene/GamePlayScene.h"

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

    SceneManager::GetInstance()->SetNextScene(SceneType::GamePlay);
}

void MyGame::Update() {
    Framework::Update();
    if (endRequst_) { return; }

    imguiManager_->Begin();

    SceneManager::GetInstance()->Update();

    imguiManager_->End();
}

void MyGame::Draw() {
    dxCommon_->PreDraw();
    srvManager_->PreDraw();

    SceneManager::GetInstance()->Draw();

    bool useGrayscale = false;
    if (auto* gamePlayScene =
            dynamic_cast<GamePlayScene*>(SceneManager::GetInstance()->GetCurrentScene())) {
        useGrayscale = gamePlayScene->IsGrayscaleEnabled();
    }
    dxCommon_->DrawRenderTextureToSwapChain(useGrayscale);

    imguiManager_->Draw();

    dxCommon_->PostDraw();
}

void MyGame::Finalize() {
    sceneFactory_.reset();
    Framework::Finalize();
}
