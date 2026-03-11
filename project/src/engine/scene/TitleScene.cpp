#include "engine/scene/TitleScene.h"

#include "engine/base/DirectXCommon.h"
#include "engine/base/SrvManager.h"
#include "engine/base/ImGuiManager.h"
#include "engine/2d/SpriteCommon.h"
#include "engine/3d/TextureManager.h"

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;


void TitleScene::Initialize() {
    TextureManager::GetInstance()->LoadTexture("resources/checkerBoard.png");

    sprites_.resize(1);
    sprites_[0].Initialize(spriteCommon_, "resources/checkerBoard.png");
    sprites_[0].SetPosition(spritePos_);
    sprites_[0].SetSize({ 640, 360 });
}

void TitleScene::Update() {
    for (auto& s : sprites_) {
        s.Update();
    }
}

void TitleScene::Draw() {

}

void TitleScene::Finalize() {
}