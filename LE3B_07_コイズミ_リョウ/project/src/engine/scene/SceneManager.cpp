#include "engine/scene/SceneManager.h"
#include "engine/scene/BaseScene.h"
#include "engine/scene/AbstractSceneFactory.h"
#include <cassert>

SceneManager* SceneManager::GetInstance() {
    static SceneManager instance;
    return &instance;
}

SceneManager::~SceneManager() {
    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
}

void SceneManager::SetSceneFactory(AbstractSceneFactory* sceneFactory) {
    sceneFactory_ = sceneFactory;
}

void SceneManager::SetSystems(
    DirectXCommon* dxCommon,
    SrvManager* srvManager,
    SpriteCommon* spriteCommon,
    ImGuiManager* imguiManager,
    Input* input
) {
    dxCommon_ = dxCommon;
    srvManager_ = srvManager;
    spriteCommon_ = spriteCommon;
    imguiManager_ = imguiManager;
    input_ = input;
}

void SceneManager::SetNextScene(SceneType sceneType) {
    nextSceneType_ = sceneType;
    hasNextScene_ = true;
}

void SceneManager::Update() {
    if (hasNextScene_) {
        if (scene_) {
            scene_->Finalize();
            scene_.reset();
        }

        assert(sceneFactory_ != nullptr);

        scene_ = sceneFactory_->CreateScene(nextSceneType_);
        assert(scene_ != nullptr);

        scene_->SetSceneManager(this);
        scene_->SetSystems(
            dxCommon_,
            srvManager_,
            spriteCommon_,
            imguiManager_,
            input_
        );
        scene_->Initialize();

        hasNextScene_ = false;
    }

    if (scene_) {
        scene_->Update();
    }
}

void SceneManager::Draw() {
    if (scene_) {
        scene_->Draw();
    }
}
