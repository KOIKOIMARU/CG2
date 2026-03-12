#include "engine/scene/TitleScene.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"
#include "engine/io/Input.h"
#include <dinput.h>

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
}

void TitleScene::Update() {
    if (input_ && input_->TriggerKey(DIK_RETURN)) {
        sceneManager_->SetNextScene(SceneType::GamePlay);
    }

    for (auto& s : sprites_) {
        s.Update();
    }
}

void TitleScene::Draw() {
}

void TitleScene::Finalize() {
}