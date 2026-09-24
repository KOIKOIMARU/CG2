// 通常起動やReleaseには自動操作を含めない。--smoke-playthrough時だけ使用。
#ifdef _DEBUG
#include "app/GameRuntime.h"
#include "engine/io/Input.h"
#include "engine/audio/SoundManager.h"
#include <imgui.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

bool GameRuntime::RunPlaythroughProbe(const std::string& logPath)
{
    static int phase = 0; // 0: 通常戦闘 1: クリア後の再挑戦待ち 2: 完了
    static int frame = 0;
    static int resultFrames = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "PLAYTHROUGH " << message << '\n';
    };
    if (!input_ || !player_ || IsTutorial()) {
        throw std::runtime_error("Playthrough requires initialized main game");
    }
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse = input_->GetMousePosition();
    if (phase == 1 && !isGameClear_) {
        if (player_->GetHp() != 100 || score_ != 0 || feverGauge_ != 0 ||
            playerShotsFired_ != 0 || !enemies_.empty() ||
            std::any_of(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), [](bool v) { return v; })) {
            throw std::runtime_error("Retry did not reset gameplay state");
        }
        if (!sound_ || sound_->GetVoiceCount() != 20 || sound_->GetPlayCount() != 0) {
            throw std::runtime_error("Retry did not recreate the bounded audio bank");
        }
        log("RETRY_RESET_OK hp=100 score=0 fever=0 voices=20");
        phase = 2;
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    if (phase == 2) {
        return true;
    }
    if (isGameOver_) {
        log("FAIL player_dead progress=" + std::to_string(stageProgress_));
        throw std::runtime_error("Playthrough pilot was defeated; no invulnerability or forced clear used");
    }
    if (frame == 0) {
        if (!sound_ || sound_->GetVoiceCount() != 20) {
            throw std::runtime_error("Combat audio bank failed to load all 20 voices");
        }
        log("START normal_damage=1 normal_collisions=1 audio_voices=20");
    }
    ++frame;
    if (frame == 61 && !showControlsHelp_) {
        throw std::runtime_error("H did not open controls help");
    }
    if (frame == 301 && showControlsHelp_) {
        throw std::runtime_error("H did not close controls help");
    }
    if (frame == 61) {
        log("HELP_OPEN_OK");
    } else if (frame == 301) {
        log("HELP_CLOSE_OK");
    }
    if (frame == 60 || frame == 300) {
        keys[DIK_H] = 0x80;
    }
    if (isGameClear_) {
        if (!bossDefeated_ || !std::all_of(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), [](bool v) { return v; })) {
            throw std::runtime_error("Clear without full schedule and boss defeat");
        }
        if (resultTransitionTimer_ <= 0 && ++resultFrames >= 120 && phase == 0) {
            log("CLEAR hp=" + std::to_string(player_->GetHp()) +
                " defeated=" + std::to_string(defeatedEnemyCount_) +
                " escaped=" + std::to_string(escapedEnemyCount_) +
                " shots=" + std::to_string(playerShotsFired_) +
                " hits=" + std::to_string(playerHitCount_) +
                " fever=" + std::to_string(feverActivationCount_) +
                " sounds=" + std::to_string(sound_->GetPlayCount()) +
                " voices=" + std::to_string(sound_->GetVoiceCount()));
            keys[DIK_R] = 0x80; // 実際の結果画面ショートカットから再挑戦する。
            phase = 1;
        }
        input_->SetTestFrame(keys, mouse);
        return false;
    }
    if (frame % 600 == 0) {
        log("PROGRESS stage=" + std::to_string(stageProgress_) +
            " hp=" + std::to_string(player_->GetHp()) +
            " defeated=" + std::to_string(defeatedEnemyCount_));
    }
    const Enemy* target = nullptr;
    for (const auto& enemy : enemies_) {
        Math::Vector2 screen{};
        if (enemy && !enemy->IsDead() && enemy->IsTargetable() &&
            TryProjectToScreen(enemy->GetAimPosition(), screen)) {
            target = enemy.get();
            const ImVec2 origin = ImGui::GetMainViewport()->Pos;
            mouse = { screen.x - origin.x, screen.y - origin.y };
            break;
        }
    }
    if (target) {
        // 硬い敵へはチャージ、それ以外には連射。性能・弾・敵HPは通常値のまま。
        if (target->GetHp() < 7 || chargeTimer_ >= chargeShotThreshold_ || feverTimer_ > 0) {
            keys[DIK_SPACE] = 0x80;
        }
    }
    const auto position = player_->GetTranslate();
    const float targetX = 4.2f * std::sin(static_cast<float>(frame) * 0.011f);
    const float targetY = 0.7f + 1.0f * std::sin(static_cast<float>(frame) * 0.017f);
    if (std::abs(targetX - position.x) > 0.3f) {
        keys[targetX > position.x ? DIK_D : DIK_A] = 0x80;
    }
    if (std::abs(targetY - position.y) > 0.3f) {
        keys[targetY > position.y ? DIK_W : DIK_S] = 0x80;
    }
    for (const auto& bullet : enemyBullets_) {
        const auto bulletPosition = bullet->GetTranslate();
        const Math::Vector3 delta{ bulletPosition.x - position.x,
            bulletPosition.y - position.y, bulletPosition.z - position.z };
        if (!bullet->IsDead() && delta.z > 0.0f && delta.z < 12.0f &&
            std::abs(delta.x) < 3.0f && std::abs(delta.y) < 2.5f && frame % 2 == 0) {
            keys[DIK_LSHIFT] = 0x80;
            break;
        }
    }
    if (feverGauge_ >= 100 && frame % 2 == 0) {
        keys[DIK_E] = 0x80;
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}
#endif
