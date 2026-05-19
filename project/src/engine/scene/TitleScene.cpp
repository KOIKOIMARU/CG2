#include "engine/scene/TitleScene.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"
#include "engine/io/Input.h"
#include <dinput.h>
#include <imgui.h>

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
}

void TitleScene::Update() {
    if (input_ && input_->TriggerKey(DIK_RETURN)) {
        sceneManager_->SetNextScene(SceneType::Game);
    }
    if (input_ && input_->TriggerKey(DIK_F2)) {
        sceneManager_->SetNextScene(SceneType::GamePlay);
    }

    ImGui::SetNextWindowPos(ImVec2(350.0f, 180.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(580.0f, 250.0f), ImGuiCond_Always);
    ImGui::Begin("Title", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::TextUnformatted("3D Rail Shooting");
    ImGui::Separator();
    ImGui::TextUnformatted("Enter: Start");
    ImGui::TextUnformatted("F2: Editor");
    ImGui::TextUnformatted("Move: WASD / Arrow Keys");
    ImGui::TextUnformatted("Shot: Space");
    ImGui::End();

    for (auto& s : sprites_) {
        s.Update();
    }
}

void TitleScene::Draw() {
}

void TitleScene::Finalize() {
}
