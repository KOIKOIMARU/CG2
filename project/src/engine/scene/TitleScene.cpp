#include "engine/scene/TitleScene.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"
#include "engine/editor/EditorManager.h"
#include "engine/io/Input.h"
#include <dinput.h>
#include <imgui.h>

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize() {
}

void TitleScene::Update() {
    if (input_ && input_->TriggerKey(DIK_RETURN)) {
        EditorManager::RequestPlayOnNextEditorOpen();
        sceneManager_->SetNextScene(SceneType::Editor);
    }
    if (input_ && input_->TriggerKey(DIK_F2)) {
        sceneManager_->SetNextScene(SceneType::Editor);
    }

    ImGui::SetNextWindowPos(ImVec2(350.0f, 180.0f), ImGuiCond_Always);
    ImGui::SetNextWindowSize(ImVec2(580.0f, 250.0f), ImGuiCond_Always);
    ImGui::Begin("タイトル", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::TextUnformatted("3Dレールシューティング");
    ImGui::Separator();
    ImGui::TextUnformatted("Enter: ゲーム開始");
    ImGui::TextUnformatted("F2: エディター");
    ImGui::TextUnformatted("移動: WASD / 方向キー");
    ImGui::TextUnformatted("ショット: Space");
    ImGui::End();

    for (auto& s : sprites_) {
        s.Update();
    }
}

void TitleScene::Draw() {
}

void TitleScene::Finalize() {
}
