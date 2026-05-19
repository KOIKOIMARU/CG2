#include "app/ResultScene.h"

#include "app/GameResult.h"
#include "engine/io/Input.h"
#include "engine/scene/SceneManager.h"

#include <imgui.h>

ResultScene::ResultScene() = default;
ResultScene::~ResultScene() = default;

void ResultScene::Initialize()
{
}

void ResultScene::Finalize()
{
}

void ResultScene::Update()
{
    if (input_ && input_->TriggerKey(DIK_RETURN)) {
        sceneManager_->SetNextScene(SceneType::Game);
        return;
    }
    if (input_ && input_->TriggerKey(DIK_T)) {
        sceneManager_->SetNextScene(SceneType::Title);
    }

    ImGui::SetNextWindowPos(ImVec2(390.0f, 210.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(500.0f, 220.0f), ImGuiCond_Always);
    ImGui::Begin("Result", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::TextUnformatted(GameResult::IsClear() ? "GAME CLEAR" : "GAME OVER");
    ImGui::Separator();
    ImGui::Text("Score: %d", GameResult::GetScore());
    ImGui::TextUnformatted("Enter: Retry");
    ImGui::TextUnformatted("T: Title");
    ImGui::End();
}

void ResultScene::Draw()
{
}
