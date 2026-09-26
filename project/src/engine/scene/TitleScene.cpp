#include "engine/scene/TitleScene.h"
#include "app/CombatHud.h"

#include "engine/io/Input.h"
#include "engine/scene/GameScene.h"
#include "engine/scene/SceneManager.h"
#include "engine/scene/SceneType.h"

#include <dinput.h>
#include <imgui.h>
#include <cstdio>

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

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    const float scale = CombatHud::Scale({ viewport->Size.x, viewport->Size.y });
    // 1280x720の組版を中央に収め、縦横比が違っても左右の項目を重ねない。
    const ImVec2 origin(viewport->Pos.x + (viewport->Size.x - 1280.0f * scale) * 0.5f,
        viewport->Pos.y + (viewport->Size.y - 720.0f * scale) * 0.5f);
    const auto p = [&](float x, float y) { return ImVec2(origin.x + x * scale, origin.y + y * scale); };
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("##SortieMenu", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    draw->AddRectFilled(viewport->Pos, { viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y },
        IM_COL32(9, 20, 31, 255));
    draw->AddRectFilledMultiColor(p(0, 0), p(1280, 720), IM_COL32(20, 47, 63, 120),
        IM_COL32(9, 20, 31, 0), IM_COL32(9, 20, 31, 0), IM_COL32(9, 20, 31, 0));
    draw->AddRectFilled(p(64, 48), p(68, 68), CombatHud::Blue);
    CombatHud::Text(draw, p(84, 47), 20.0f * scale, CombatHud::Muted, "3Dレールシューティング");
    CombatHud::Text(draw, p(64, 142), 58.0f * scale, CombatHud::White, "出撃準備");
    CombatHud::Text(draw, p(66, 226), 19.0f * scale, CombatHud::Muted, "敵編隊を突破し、最深部のボスを撃破。");

    const auto button = [&](const char* id, float y, const char* label, const char* key, bool primary) {
        const ImVec2 min = p(64, y), max = p(500, y + 62);
        ImGui::SetCursorScreenPos(min);
        const bool pressed = ImGui::InvisibleButton(id, { max.x - min.x, max.y - min.y });
        const bool lit = primary || ImGui::IsItemHovered();
        const ImU32 fill = ImGui::IsItemHovered() ? CombatHud::Blue : (primary ? CombatHud::White : IM_COL32(20, 36, 49, 255));
        const ImU32 textColor = lit ? IM_COL32(9, 20, 31, 255) : CombatHud::White;
        draw->AddRectFilled(min, max, fill);
        draw->AddRectFilled(min, p(68, y + 62), CombatHud::Blue);
        CombatHud::Text(draw, p(86, y + 13), 27.0f * scale, textColor, label);
        CombatHud::Text(draw, p(476, y + 18), 26.0f * scale, textColor, key, true, true);
        return pressed;
    };
    if (button("start", 310, "出撃する", "ENTER", true)) { RequestStart(); }
    if (button("tutorial", 390, "操作練習", "T", false)) { RequestStart(true); }
    CombatHud::Text(draw, p(66, 486), 16.0f * scale, CombatHud::Muted, "操作練習は、本編とは別のステージです。");
    CombatHud::Text(draw, p(66, 516), 16.0f * scale, CombatHud::Muted, "本編中も H キーで操作を確認できます。");

    CombatHud::Text(draw, p(730, 153), 25.0f * scale, CombatHud::White, "操作");
    const char* labels[] = { "移動", "照準", "射撃", "回避", "残像連撃", "フィーバー" };
    const char* values[] = { "WASD / 方向キー", "マウス", "SPACE", "A・D + SHIFT", "Q", "ゲージ満タンで自動" };
    for (int index = 0; index < 6; ++index) {
        const float y = 215.0f + static_cast<float>(index) * 48.0f;
        CombatHud::Text(draw, p(730, y), 18.0f * scale, CombatHud::Muted, labels[index]);
        CombatHud::Text(draw, p(1190, y), 19.0f * scale, CombatHud::White, values[index], true);
        draw->AddLine(p(730, y + 34), p(1190, y + 34), IM_COL32(51, 72, 87, 255), scale);
    }
    CombatHud::Text(draw, p(730, 530), 16.0f * scale, CombatHud::Muted, "SPACEを離してチャージ。硬い敵に有効。");
    CombatHud::Text(draw, p(730, 558), 16.0f * scale, CombatHud::Muted, "残像連撃は8秒で回復。射撃命中で回復を短縮。");
    const float preload = resourcesReady ? 1.0f : std::clamp(
        static_cast<float>(GameScene::GetResourcePreloadStep()) /
        static_cast<float>((std::max)(1, GameScene::GetResourcePreloadStepCount())), 0.0f, 1.0f);
    const char* state = startRequested_ ? "出撃準備中" : (gameReady ? "準備完了" : "リソース読み込み中");
    CombatHud::Text(draw, p(64, 648), 15.0f * scale, CombatHud::Muted, state);
    char progress[24]{};
    std::snprintf(progress, sizeof(progress), "%d%%", static_cast<int>(preload * 100.0f));
    CombatHud::Text(draw, p(1190, 642), 26.0f * scale, CombatHud::Blue, progress, true, true);
    draw->AddRectFilled(p(64, 680), p(1190, 683), IM_COL32(33, 53, 67, 255));
    draw->AddRectFilled(p(64, 680), p(64.0f + 1126.0f * preload, 683), CombatHud::Blue);
    ImGui::End();
    ImGui::PopStyleVar();

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
