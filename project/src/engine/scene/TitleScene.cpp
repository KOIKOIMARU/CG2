#include "engine/scene/TitleScene.h"

#include "engine/io/Input.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

#include <dinput.h>
#include <imgui.h>

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
}

void TitleScene::Update()
{
    if (input_ && input_->TriggerKey(DIK_RETURN)) {
        sceneManager_->SetNextScene(SceneType::Game);
    }

    ImGui::SetNextWindowPos(ImVec2(350.0f, 180.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(580.0f, 230.0f), ImGuiCond_Always);
    ImGui::Begin("タイトル", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::TextUnformatted("3Dレールシューティング");
    ImGui::Separator();
    ImGui::TextUnformatted("Enter: ゲーム開始");
    ImGui::TextUnformatted("移動: WASD / 方向キー");
    ImGui::TextUnformatted("ショット: Space");
    ImGui::TextUnformatted("F2: タイトルへ戻る");
    ImGui::End();

    for (auto& s : sprites_) {
        s.Update();
    }
}

void TitleScene::Draw()
{
}

void TitleScene::Finalize()
{
}
