#include "engine/scene/TitleScene.h"

#include "engine/io/Input.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

#include <dinput.h>
#include <imgui.h>

TitleScene::TitleScene() = default;
TitleScene::~TitleScene() = default;

void TitleScene::Initialize()
{
    startRequested_ = false;
    requestedScene_ = SceneType::Game;
}

void TitleScene::RequestStart(bool tutorial)
{
    requestedScene_ = tutorial ? SceneType::Tutorial : SceneType::Game;
    startRequested_ = true;
}

void TitleScene::Update()
{
    if (input_ && input_->TriggerKey(DIK_RETURN)) {
        RequestStart();
    } else if (input_ && input_->TriggerKey(DIK_T)) {
        RequestStart(true);
    }
    const bool resourcesReady =
        GameScene::PreloadResourcesStep(dxCommon_, srvManager_);
    if (resourcesReady && sceneManager_ &&
        !sceneManager_->IsScenePrepared(requestedScene_)) {
        sceneManager_->PrepareScene(requestedScene_);
    }
    const bool gameReady =
        sceneManager_ && sceneManager_->IsScenePrepared(requestedScene_);

    if (startRequested_ && gameReady && sceneManager_) {
        sceneManager_->SetNextScene(requestedScene_);
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(660.0f, 385.0f), ImGuiCond_Always);
    ImGui::Begin("タイトル", nullptr, ImGuiWindowFlags_NoResize);
    ImGui::TextUnformatted("3Dレールシューティング");
    ImGui::Separator();
    if (ImGui::Button("ゲーム開始 [Enter]", ImVec2(260.0f, 38.0f))) {
        RequestStart();
    }
    ImGui::SameLine();
    if (ImGui::Button("チュートリアル [T]", ImVec2(260.0f, 38.0f))) {
        RequestStart(true);
    }
    ImGui::TextUnformatted("チュートリアルは操作練習用です。本編とは別に遊べます。");
    ImGui::Separator();
    ImGui::TextUnformatted("目的: 道中の敵を突破し、最後のボスを撃破する");
    ImGui::Spacing();
    ImGui::TextUnformatted("移動: WASD / 方向キー    照準: マウス");
    ImGui::TextUnformatted("連射: SPACE長押し    チャージ: SPACEを離してため、次の一発");
    ImGui::TextUnformatted("回避: A・D + SHIFT    敵弾の近くで回避するとジャスト回避");
    ImGui::TextUnformatted("フィーバー: ゲージ満タンで自動発動    連射・威力・スピードを強化");
    ImGui::TextUnformatted("残像連撃: Q / 再使用8秒・射撃命中で短縮 / フィーバー中は5連撃");
    ImGui::Spacing();
    ImGui::TextUnformatted("連射で削る / 硬い敵にはチャージ / 危険な弾は回避で抜ける");
    ImGui::TextUnformatted("ボスの攻撃後に出る「反撃チャンス」はダメージ2倍");
    ImGui::Separator();
    ImGui::TextUnformatted("H: 操作を確認    F2: タイトルへ戻る    結果画面で R: 再挑戦");
    if (!resourcesReady) {
        ImGui::Separator();
        ImGui::Text(
            "Loading game assets: %d / %d",
            GameScene::GetResourcePreloadStep(),
            GameScene::GetResourcePreloadStepCount());
        ImGui::TextUnformatted(GameScene::GetResourcePreloadLabel());
        ImGui::Text(
            "last %.1f ms / total %.1f ms",
            GameScene::GetResourcePreloadLastStepMs(),
            GameScene::GetResourcePreloadTotalMs());
    } else if (!gameReady || startRequested_) {
        ImGui::Separator();
        ImGui::TextUnformatted("Preparing game...");
    }
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
