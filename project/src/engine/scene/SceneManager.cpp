#include "engine/scene/SceneManager.h"
#include "engine/scene/BaseScene.h"
#include "engine/scene/AbstractSceneFactory.h"
#include <stdexcept>

SceneManager* SceneManager::GetInstance() {
    // シーン遷移の管理元を一つに限定する。所有シーンはunique_ptrで保持する。
    static SceneManager instance;
    return &instance;
}

SceneManager::~SceneManager() {
    FinalizeCurrentScene();
    if (preparedScene_) {
        preparedScene_->Finalize();
        preparedScene_.reset();
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

void SceneManager::PrepareScene(SceneType sceneType) {
    // タイトル画面などで次シーンを先に初期化し、遷移時の読み込み待ちを減らす。
    if (hasPreparedScene_ && preparedSceneType_ == sceneType) {
        return;
    }

    if (preparedScene_) {
        preparedScene_->Finalize();
        preparedScene_.reset();
    }

    if (!sceneFactory_) {
        throw std::logic_error("SceneManager requires a scene factory");
    }
    std::unique_ptr<BaseScene> scene = sceneFactory_->CreateScene(sceneType);
    if (!scene) {
        throw std::runtime_error("Scene factory failed to create a scene");
    }
    scene->SetSceneManager(this);
    scene->SetSystems(
        dxCommon_,
        srvManager_,
        spriteCommon_,
        imguiManager_,
        input_);
    scene->Initialize();
    preparedScene_ = std::move(scene);
    preparedSceneType_ = sceneType;
    hasPreparedScene_ = true;
}

bool SceneManager::IsScenePrepared(SceneType sceneType) const {
    return hasPreparedScene_ && preparedSceneType_ == sceneType;
}

void SceneManager::FinalizeCurrentScene()
{
    if (scene_) {
        scene_->Finalize();
        scene_.reset();
    }
    hasNextScene_ = false;
}

void SceneManager::Update() {
    if (hasNextScene_) {
        // シーンのFinalizeとInitializeが同じフレームで重複しないよう、ここで遷移を確定する。
        FinalizeCurrentScene();

        if (hasPreparedScene_ && preparedSceneType_ == nextSceneType_) {
            // 準備済みシーンは再初期化せず、そのまま所有権を現在シーンへ移す。
            scene_ = std::move(preparedScene_);
            hasPreparedScene_ = false;
        } else {
            if (!sceneFactory_) {
                throw std::logic_error("SceneManager requires a scene factory");
            }
            std::unique_ptr<BaseScene> scene =
                sceneFactory_->CreateScene(nextSceneType_);
            if (!scene) {
                throw std::runtime_error("Scene factory failed to create a scene");
            }

            scene->SetSceneManager(this);
            scene->SetSystems(
                dxCommon_,
                srvManager_,
                spriteCommon_,
                imguiManager_,
                input_
            );
            scene->Initialize();
            scene_ = std::move(scene);
        }

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
