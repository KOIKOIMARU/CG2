#include "app/MyGame.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/TitleScene.h"
#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/base/ImGuiManager.h"

MyGame::MyGame() = default;
MyGame::~MyGame() = default;

void MyGame::Initialize() {
    Framework::Initialize();

    sceneManager_ = std::make_unique<SceneManager>();

    auto titleScene = std::make_unique<TitleScene>();
    titleScene->SetSystems(
        dxCommon_.get(),
        srvManager_.get(),
        spriteCommon_.get(),
        imguiManager_.get()
    );

    sceneManager_->SetNextScene(std::move(titleScene));
}

void MyGame::Update() {
    Framework::Update();
    if (endRequst_) { return; }

    imguiManager_->Begin();

    if (sceneManager_) {
        sceneManager_->Update();
    }

    imguiManager_->End();
}
void MyGame::Draw() {
    dxCommon_->PreDraw();
    srvManager_->PreDraw();

    if (sceneManager_) {
        sceneManager_->Draw();
    }

    imguiManager_->Draw();

    dxCommon_->PostDraw();
}

void MyGame::Finalize() {
    sceneManager_.reset();
    Framework::Finalize();
}