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

namespace {
// 実際のPlayerと入力経路を使う回避回帰試験。描画しない独立自機なので本編へ影響しない。
void CheckDodgeControls(Object3dCommon* common, Model* model)
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    };
    for (const float step : { 0.5f, 1.0f, 2.0f }) {
        for (const float slow : { 1.0f, 0.08f }) {
            Player probe;
            Input input;
            probe.Initialize(common, model);
            const int frames = static_cast<int>(16.0f / step);
            for (int frame = 0; frame < frames; ++frame) {
                std::array<BYTE, 256> keys{};
                if (frame == 0) { keys[DIK_D] = keys[DIK_LSHIFT] = 0x80; }
                input.SetTestFrame(keys, {});
                probe.Update(&input, slow, step);
                require(probe.IsInvincible(), "dodge lost protection before movement ended");
                if (frame == 0 && step == 1.0f) {
                    require(probe.GetTranslate().x > 0.50f, "dodge initial response too slow");
                }
            }
            require(!probe.IsDodging() && std::abs(probe.GetTranslate().x - 4.4f) < 0.001f,
                "dodge distance/duration depends on slow motion or frame rate");
            input.SetTestFrame({}, {});
            probe.Update(&input, slow, step);
            require(!probe.IsInvincible(), "dodge protection remained after finish");
        }
    }
    for (const bool nearReady : { false, true }) {
        Player probe;
        Input input;
        probe.Initialize(common, model);
        for (int frame = 1; frame <= 29; ++frame) {
            std::array<BYTE, 256> keys{};
            if (frame == 1) { keys[DIK_D] = keys[DIK_LSHIFT] = 0x80; }
            if (frame == (nearReady ? 25 : 19)) { keys[DIK_A] = keys[DIK_LSHIFT] = 0x80; }
            if (frame == 26) { keys[DIK_D] = 0x80; } // 方向を変えても予約した左回避は維持。
            input.SetTestFrame(keys, {});
            probe.Update(&input);
        }
        require(probe.IsDodging() == nearReady, "dodge input buffer did not expire/activate correctly");
        if (nearReady) { require(probe.GetDodgeDirection() == -1, "buffered dodge direction changed"); }
    }
    Player edge;
    Input input;
    edge.Initialize(common, model);
    std::array<BYTE, 256> right{};
    right[DIK_D] = 0x80;
    input.SetTestFrame(right, {});
    for (int frame = 0; frame < 60; ++frame) { edge.Update(&input); }
    const float limit = edge.GetTranslate().x;
    for (int frame = 0; frame < 16; ++frame) {
        std::array<BYTE, 256> keys{};
        if (frame == 0) { keys[DIK_D] = keys[DIK_LSHIFT] = 0x80; }
        input.SetTestFrame(keys, {});
        edge.Update(&input);
        require(std::abs(edge.GetTranslate().x - limit) < 0.001f, "dodge crossed playfield boundary");
    }
    std::array<BYTE, 256> left{};
    left[DIK_A] = 0x80;
    input.SetTestFrame(left, {});
    edge.Update(&input, 0.08f);
    require(!edge.IsDodging() && edge.GetTranslate().x < limit - 0.15f,
        "movement did not resume promptly after edge dodge in slow motion");
}
} // namespace

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
            feverTimer_ != 0 || feverActivationCount_ != 0 ||
            playerShotsFired_ != 0 || !enemies_.empty() ||
            std::any_of(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), [](bool v) { return v; })) {
            throw std::runtime_error("Retry did not reset gameplay state");
        }
        if (!sound_ || sound_->GetVoiceCount() != 25 || sound_->GetPlayCount() != 0) {
            throw std::runtime_error("Retry did not recreate the bounded audio bank");
        }
        log("RETRY_RESET_OK hp=100 score=0 fever=0 voices=25");
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
        if (!sound_ || sound_->GetVoiceCount() != 25) {
            throw std::runtime_error("Combat audio bank failed to load all 25 voices");
        }
        log("START normal_damage=1 normal_collisions=1 audio_voices=25");
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
            if (feverActivationCount_ <= 0) {
                throw std::runtime_error("Fever did not auto activate during normal combat");
            }
            log("FEVER_AUTO_OK no_activation_key=1");
            log("CLEAR hp=" + std::to_string(player_->GetHp()) +
                " defeated=" + std::to_string(defeatedEnemyCount_) +
                " escaped=" + std::to_string(escapedEnemyCount_) +
                " shots=" + std::to_string(playerShotsFired_) +
                " hits=" + std::to_string(playerHitCount_) +
                " fever=" + std::to_string(feverActivationCount_) +
                " phantom=" + std::to_string(phantomActivationCount_) +
                " phantom_kills=" + std::to_string(phantomDefeatCount_) +
                " pool_miss_p=" + std::to_string(playerBulletPoolMisses_) +
                " pool_miss_e=" + std::to_string(enemyBulletPoolMisses_) +
                " pool_miss_fx=" + std::to_string(hitEffectObjectPoolMisses_) +
                " max_bullets_p=" + std::to_string(maxActivePlayerBullets_) +
                " max_bullets_e=" + std::to_string(maxActiveEnemyBullets_) +
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
    if (phantomReady_ && target && frame % 2 == 0) {
        keys[DIK_Q] = 0x80;
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}

bool GameRuntime::RunPhantomProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    static bool retry = false;
    static float dodgeStartX = 0.0f;
    static int feverCountBeforeAutoTest = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "PHANTOM_TEST " << message << '\n';
    };
    const auto require = [&](bool condition, const char* message) {
        if (!condition) {
            log(std::string("FAIL ") + message);
            throw std::runtime_error(message);
        }
    };
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse{ 640.0f, 300.0f };
    if (preview && (frame == 0 || frame == 31 || frame == 38 || frame == 46 ||
        frame == 172 || frame == 184 || frame == 205 || frame == 230 || frame == 359 || frame == 374)) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
        ImGui::Begin("Phantom visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("TEST ONLY: animation frozen for visual inspection");
        ImGui::Text("Sample: %d", frame);
        const bool next = ImGui::Button("NEXT / resume test", { 220.0f, 36.0f });
        ImGui::End();
        if (!next) {
            // 表示確認中も長押し試験の入力を保つ。ここで離すと再開時に
            // 新しいSHIFT押下が生まれ、通常プレイにはない再回避を起こしてしまう。
            if (frame >= 31 && frame <= 65) { keys[DIK_LSHIFT] = 0x80; }
            input_->SetTestFrame(keys, mouse);
            return false;
        }
        phantomPreviewPaused_ = false;
    }
    if (retry && !isGameClear_) {
        require(phantomReady_ && !IsPhantomRaidActive() && phantomActivationCount_ == 0 &&
            phantomDefeatCount_ == 0 && phantomCooldown_ == 0.0f, "retry skill state reset");
        require(sound_ && sound_->GetVoiceCount() == 25, "retry audio pool");
        log("PASS retry_reset voices=25");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    ++frame;
    require(!isGameOver_, "fixture player died");
    if (frame == 1) {
        CheckDodgeControls(object3dCommon_.get(), playerModel_);
        log("DODGE_CONTROLS_OK distance=4.4 duration=16 buffer=5 slow_independent=1 steps=0.5,1,2 edge_ok=1");
        require(phantomReady_ && !IsPhantomRaidActive() && phantomCooldown_ == 0.0f,
            "startup must be ready without dodge, but must not auto activate");
        require(sound_ && sound_->GetVoiceCount() == 25, "all sounds loaded");
        std::fill(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), true);
        log("BEGIN controlled_enemy_fixture=1 normal_damage_code=1");
        keys[DIK_Q] = 0x80;
    }
    if (frame == 2) {
        require(phantomActivationCount_ == 0 && phantomReady_ && phantomCooldown_ == 0.0f,
            "Q without target consumed startup ready");
        log("STARTUP_READY_WITHOUT_DODGE_OK empty_target_no_consumption=1");
    }
    if (frame == 30) {
        phantomReady_ = false;
        phantomCooldown_ = 300.0f; // 回避でスキルが再充填されないことを独立検証。
        // 実際の当たり判定を通る近接弾を置き、SHIFTでジャスト回避させる。
        const auto p = player_->GetTranslate();
        dodgeStartX = p.x;
        // 初動の横移動量に左右されないよう、上側のかすり領域へ置く。
        // 機体に直撃する配置では「無敵で弾消去」になり、ジャスト回避試験にならない。
        FireEnemyBullet({ p.x + 0.5f, p.y + 2.2f, p.z + 1.0f });
        keys[DIK_D] = keys[DIK_LSHIFT] = 0x80;
    }
    // 実フレーム間でSHIFTを押し続けても、再使用可能時に勝手に2回目が出ない。
    if (frame >= 31 && frame <= 65) { keys[DIK_LSHIFT] = 0x80; }
    if (frame == 31) {
        require(justDodgeCount_ > 0 && !phantomReady_ && phantomCooldown_ > 290.0f,
            "just dodge unexpectedly recharged the independent skill");
        for (auto& bullet : enemyBullets_) { bullet->Kill(); }
        phantomCooldown_ = 0.0f;
        GrantPhantomRaid(); // 後続の連撃試験を初期の使用可能状態へ戻す。
        log("DODGE_INDEPENDENT_OK");
    }
    if (frame == 40) { keys[DIK_Q] = 0x80; }
    if (frame == 41) {
        require(phantomReady_ && !IsPhantomRaidActive(), "no target consumed ready");
        log("NO_TARGET_NO_CONSUMPTION_OK");
    }
    if (frame == 65) {
        require(!player_->IsDodging() &&
            std::abs(player_->GetTranslate().x - dodgeStartX - 4.4f) < 0.01f,
            "held SHIFT caused an automatic repeat dodge");
        log("DODGE_HELD_KEY_NO_REPEAT_OK");
    }
    const auto spawn = [&](int count) {
        for (int index = 0; index < count; ++index) {
            const float x = (static_cast<float>(index) - static_cast<float>(count - 1) * 0.5f) * 2.6f;
            SpawnStageEnemy(x, 1.0f + static_cast<float>(index % 2) * 0.7f, 32.0f,
                Enemy::Behavior::Formation, Enemy::EntryStyle::Direct, 3, 1.0f);
        }
    };
    if (frame == 70) { spawn(3); }
    if (frame == 150) { keys[DIK_Q] = 0x80; log("NORMAL_ACTIVATE"); }
    if (frame == 151) {
        require(IsPhantomRaidActive() && phantomTargetCount_ == 3, "normal target selection");
        require(phantomCooldown_ == kPhantomCooldownFrames, "activation must begin 8-second cooldown");
        const float before = phantomCooldown_;
        RecoverPhantomRaidOnHit(true, true);
        require(phantomCooldown_ == before, "active skill allowed cooldown recovery");
        log("COOLDOWN_8_SECONDS_OK no_self_recharge=1");
    }
    if (frame == 220) {
        require(!IsPhantomRaidActive() && phantomDefeatCount_ == 3 && defeatedEnemyCount_ == 3, "normal triple finish");
        require(!phantomReady_ && phantomCooldown_ > 0.0f, "normal consumption or cooldown");
        GrantPhantomRaid();
        require(!phantomReady_, "cooldown allowed immediate recharge");
        const float before = phantomCooldown_;
        RecoverPhantomRaidOnHit(false, false);
        require(std::abs(phantomCooldown_ - (before - 12.0f)) < 0.001f, "normal hit cooldown reduction");
        RecoverPhantomRaidOnHit(true, true);
        require(std::abs(phantomCooldown_ - (before - 66.0f)) < 0.001f, "charged kill cooldown reduction");
        phantomCooldown_ = 6.0f;
        RecoverPhantomRaidOnHit(false, false);
        require(phantomReady_ && phantomCooldown_ == 0.0f, "hit did not complete recovery");
        const auto sounds = sound_->GetPlayCount();
        RecoverPhantomRaidOnHit(true, true);
        require(sound_->GetPlayCount() == sounds, "ready sound repeated on extra hit");
        log("NORMAL_3_KILLS_OK reward_once cooldown_ok");
        log("HIT_RECOVERY_OK normal=12 charged_kill=54 clamped=0 notification_once=1");
    }
    if (frame == 221) {
        phantomReady_ = false;
        phantomCooldown_ = 3.0f; // 最後の短区間を無入力で進め、満了時の自動回復を検証。
    }
    if (frame == 230) {
        require(phantomReady_ && phantomCooldown_ == 0.0f, "time expiry did not auto recharge skill");
        log("TIME_ONLY_RECOVERY_OK no_shooting_or_dodge=1");
    }
    if (frame == 232) {
        feverTimer_ = 0;
        feverGauge_ = 99;
        feverCountBeforeAutoTest = feverActivationCount_;
    }
    if (frame == 233) {
        require(feverTimer_ == 0 && feverActivationCount_ == feverCountBeforeAutoTest,
            "fever activated below full gauge");
        AddFeverGauge(1); // 本番のゲージ加算経路で満タンにする。Eキーは送らない。
    }
    if (frame == 234) {
        require(feverTimer_ > 0 && feverGauge_ == 0 &&
            feverActivationCount_ == feverCountBeforeAutoTest + 1,
            "full gauge did not auto activate fever exactly once");
        AddFeverGauge(100);
    }
    if (frame == 235) {
        require(feverGauge_ == 0 && feverActivationCount_ == feverCountBeforeAutoTest + 1,
            "active fever accumulated gauge or activated twice");
        log("FEVER_AUTO_OK threshold=100 gauge_consumed=1 no_key=1 no_double_activation=1");
    }
    if (frame == 240) {
        phantomCooldown_ = 0.0f; // 次の独立ケース用の試験リセット。
        GrantPhantomRaid();
        spawn(5);
    }
    if (frame == 330) { keys[DIK_Q] = 0x80; log("OVERDRIVE_ACTIVATE"); }
    if (frame == 331) { require(phantomEmpowered_ && phantomTargetCount_ == 5, "fever target selection"); }
    if (frame == 410) {
        require(!IsPhantomRaidActive() && phantomDefeatCount_ == 8 && defeatedEnemyCount_ == 8, "fever five finish");
        log("OVERDRIVE_5_KILLS_OK");
    }
    if (frame == 430) {
        feverTimer_ = 1; // 終了境界も通常のUpdateFever経路で検証する。
        phantomCooldown_ = 0.0f;
        GrantPhantomRaid();
        spawn(3);
    }
    if (frame == 432) {
        require(feverTimer_ == 0 && feverGauge_ == 0 &&
            feverActivationCount_ == feverCountBeforeAutoTest + 1,
            "fever reactivated after expiry without refilling");
        log("FEVER_AUTO_EXPIRY_OK no_retrigger=1");
    }
    if (frame == 520) { keys[DIK_Q] = 0x80; log("TARGET_REMOVAL_ACTIVATE"); }
    if (frame == 526) {
        for (auto& enemy : enemies_) { enemy->Kill(); } // 演出途中の外部消滅を模擬。
    }
    if (frame == 590) {
        require(!IsPhantomRaidActive() && phantomDefeatCount_ == 8, "removed targets double rewarded");
        log("REMOVED_TARGETS_SAFE_OK");
    }
    if (frame == 620) {
        DebugJumpToStagePhase(3);
        log("BOSS_FIXTURE_BEGIN");
    }
    if (frame == 820) {
        Enemy* boss = nullptr;
        for (auto& enemy : enemies_) { if (enemy->IsBoss()) { boss = enemy.get(); break; } }
        require(boss != nullptr, "boss fixture missing");
        boss->Damage((std::max)(0, boss->GetHp() - 7)); // クリア経路を調べる低HP試験配置。
        GrantPhantomRaid();
        ActivateFever();
    }
    if (frame == 850) { keys[DIK_Q] = 0x80; log("BOSS_FINISH_ACTIVATE"); }
    if (frame > 920 && isGameClear_ && resultTransitionTimer_ <= 0 && !retry) {
        require(bossDefeated_ && phantomDefeatCount_ == 1 && defeatedEnemyCount_ == 27, "boss clear reward path");
        log("BOSS_SKILL_CLEAR_OK");
        keys[DIK_R] = 0x80;
        retry = true;
    }
    if (frame == 1100) { require(retry, "boss clear not reached"); }
    input_->SetTestFrame(keys, mouse);
    return false;
}
#endif
