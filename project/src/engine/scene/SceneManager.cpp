#include "engine/scene/SceneManager.h"
#include "engine/scene/BaseScene.h"

SceneManager::~SceneManager() {
    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
}

void SceneManager::SetNextScene(std::unique_ptr<BaseScene> nextScene) {
    nextScene_ = std::move(nextScene);
}

void SceneManager::Update() {
    if (nextScene_) {
        if (scene_) {
            scene_->Finalize();
            scene_.reset();
        }

        scene_ = std::move(nextScene_);
        scene_->SetSceneManager(this);
        scene_->Initialize();
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