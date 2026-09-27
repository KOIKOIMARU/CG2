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
#include <iterator>

namespace {
void CheckSniperPosture(Object3dCommon* common, Model* model)
{
    const auto require = [](bool ok, const char* message) {
        if (!ok) { throw std::runtime_error(message); }
    };
    // 実際のEnemy::Updateで構えの静止とレール追従を検証。ゲームの敵HPや無敵時間は変えない。
    Enemy sniper;
    sniper.Initialize(common, model, { 5.3f, 2.0f, 48.0f }, Enemy::Behavior::Sniper);
    float rail = 0.0f;
    for (int frame = 0; frame < 112; ++frame) {
        rail += 0.2f;
        sniper.Update(rail);
    }
    require(sniper.CanShoot(), "sniper did not finish entry");
    auto& fire = sniper.GetFireControl();
    const EnemyFireControl::Pattern pattern{ 60.0f, 18.0f, 128.0f, 2 };
    fire.Advance(1.0f, true, pattern);
    sniper.Update(rail);
    const auto anchor = sniper.GetTranslate();
    const float ahead = anchor.z - rail;
    for (int frame = 1; frame <= 120; ++frame) {
        rail += 0.45f;
        const auto event = fire.Advance(1.0f, sniper.CanShoot(), pattern);
        sniper.Update(rail);
        const auto position = sniper.GetTranslate();
        require(std::abs(position.x - anchor.x) < 0.001f, "sniper drifted laterally while braced/recovering");
        if (frame < 60) {
            require(std::abs(position.z - rail - ahead) < 0.001f, "sniper brace lost rail tracking");
        }
        if (frame == 60 || frame == 78) {
            require(event == EnemyFireControl::Event::Fire && fire.ShotFlash() == 1.0f,
                "sniper recoil is not tied to the shot");
        }
        if (frame == 110) {
            require(!fire.IsBraced() && std::abs(fire.RecoveryElapsed() - 32.0f) < 0.001f &&
                position.z - rail < ahead - 2.5f, "sniper did not expose recovery opening");
        }
    }
    sniper.Kill();
    fire.Advance(1.0f, sniper.CanShoot(), pattern);
    require(!fire.IsBraced() && fire.ShotFlash() == 0.0f, "destroyed sniper kept charging/firing");
}

void CheckEnemyFireControl()
{
    const auto require = [](bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    };
    // 通常速度・フィーバー速度・弾速とレール速度が同じ場合も、左右からの狙いがずれない。
    for (const float rail : { 0.16f, 0.235f, 0.45f, 0.65f }) {
        for (const float x : { -10.0f, 0.0f, 10.0f }) {
            const Math::Vector3 origin{ x, 4.0f, 40.0f };
            const Math::Vector3 target{ 3.0f, 0.0f, 0.0f };
            const auto velocity = EnemyFireControl::SolveShotDirection(origin, target, rail, 0.45f) * 0.45f;
            const float t = (origin.z - target.z) / (rail - velocity.z);
            const float dx = origin.x + velocity.x * t - target.x;
            const float dy = origin.y + velocity.y * t - target.y;
            require(std::isfinite(t) && t > 0 && std::abs(dx) < 0.001f && std::abs(dy) < 0.001f,
                "enemy shot misses stationary lateral target during rail movement");
            // 発射後に横へ3m移動すれば、この固定照準弾の当たり判定から抜ける。
            require(std::abs(dx - 3.0f) > 1.5f, "enemy shot followed an evasive movement");
        }
    }
    const EnemyFireControl::Pattern pattern{ 28.0f, 12.0f, 86.0f, 3 };
    for (const float step : { 0.25f, 0.5f, 1.0f }) {
        EnemyFireControl::Cycle control;
        require(control.Advance(step, true, pattern) == EnemyFireControl::Event::Aim,
            "enemy fired before windup");
        int shots = 0;
        for (float elapsed = step; elapsed <= 130.0f; elapsed += step) {
            const auto event = control.Advance(step, true, pattern);
            if (event == EnemyFireControl::Event::Fire) {
                require(std::abs(elapsed - (28.0f + shots * 12.0f)) < 0.01f,
                    "enemy burst ignored slow-motion clock");
                require(control.ShotIndex() == shots, "enemy burst order is incorrect");
                ++shots;
            }
            if (elapsed >= 21.0f && elapsed < 28.0f) {
                require(!control.IsTracking(), "enemy keeps tracking in final windup");
            }
        }
        require(shots == 3, "enemy burst count/recovery is incorrect");
        control.Advance(step, false, pattern);
        require(control.ChargeRate() == 0.0f &&
            control.Advance(step, true, pattern) == EnemyFireControl::Event::Aim,
            "offscreen enemy retained an immediate shot");
    }
}

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
        // 再入場の最初のUpdateは既に通っているので、新しい通常曲が一つだけ再生される。
        if (!sound_ || sound_->GetVoiceCount() != 28 || sound_->GetPlayCount() != 0 ||
            sound_->GetLoopCount() != 1 || musicTrack_ != 0) {
            throw std::runtime_error("Retry did not recreate the bounded audio bank");
        }
        log("RETRY_RESET_OK hp=100 score=0 fever=0 voices=28 music_loops=1 track=stage");
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
        CheckEnemyFireControl();
        CheckSniperPosture(object3dCommon_.get(), enemyShooterModel_);
        log("SNIPER_POSTURE_OK brace_still=1 rail_relative=1 recoil_on_shot=1 recovery_open=1 death_cancels=1");
        log("ENEMY_FIRE_CONTROL_OK rail_intercept=1 fixed_aim=1 windup_burst_recovery=1 slow_steps=0.25,0.5,1");
        if (!sound_ || sound_->GetVoiceCount() != 28) {
            throw std::runtime_error("Combat audio bank failed to load all 28 voices");
        }
        log("START normal_damage=1 normal_collisions=1 audio_voices=28");
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
            if (bossShotsFired_ <= 0) {
                throw std::runtime_error("Boss was defeated without firing any attack");
            }
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
                " boss_attacks_started=" + std::to_string(bossAttackSequence_) +
                " boss_shots=" + std::to_string(bossShotsFired_) +
                " boss_phase=" + std::to_string(bossPhase_) +
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
        require(sound_ && sound_->GetVoiceCount() == 28, "retry audio pool");
        log("PASS retry_reset voices=28");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    ++frame;
    require(!isGameOver_, "fixture player died");
    if (frame == 1) {
        CheckEnemyFireControl();
        log("ENEMY_FIRE_CONTROL_OK rail_intercept=1 fixed_aim=1 windup_burst_recovery=1");
        CheckDodgeControls(object3dCommon_.get(), playerModel_);
        log("DODGE_CONTROLS_OK distance=4.4 duration=16 buffer=5 slow_independent=1 steps=0.5,1,2 edge_ok=1");
        require(phantomReady_ && !IsPhantomRaidActive() && phantomCooldown_ == 0.0f,
            "startup must be ready without dodge, but must not auto activate");
        require(sound_ && sound_->GetVoiceCount() == 28, "all sounds loaded");
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
    if (frame == 70) {
        // 本編「Sniper wing」と同じ位置・HP・編隊動作で、スキル対象の選択を検証する。
        SpawnStageEnemy(-4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(-2.0f, 0.5f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation,
            Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(5.3f, 2.0f, 48.0f, Enemy::Behavior::Sniper,
            Enemy::EntryStyle::PopShooter, 6, 1.18f);
    }
    if (frame == 149) {
        // 狙撃機を指した場合も実際の選択処理を通す。時間は進めず、ダメージ前に試験状態を戻す。
        Math::Vector2 screen{};
        require(enemies_.size() == 4 && enemies_.back()->IsSniper() &&
            TryProjectToScreen(enemies_.back()->GetAimPosition(), screen), "sniper selection fixture missing");
        const auto savedReticle = reticleScreen_;
        reticleScreen_ = screen;
        require(TryActivatePhantomRaid() && phantomTargetCount_ == 3 &&
            phantomTargets_[0].enemy == enemies_.back().get(), "sniper could not be prioritized by aiming");
        ResetPhantomRaid();
        reticleScreen_ = savedReticle;
        log("SNIPER_CHOICE_OK aimed_sniper_selected_first=1");
    }
    if (frame == 150) {
        Math::Vector2 screen{};
        require(enemies_.size() == 4 && TryProjectToScreen((*std::next(enemies_.begin()))->GetAimPosition(), screen),
            "formation/sniper fixture missing");
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
        // UpdateLockOnTargetはUpdateの末尾なので、照準を置く1フレームと発動を分ける。
    }
    if (frame == 151) {
        mouse = input_->GetMousePosition();
        keys[DIK_Q] = 0x80;
        log("NORMAL_ACTIVATE");
    }
    if (frame == 152) {
        require(IsPhantomRaidActive() && phantomTargetCount_ == 3, "normal target selection");
        for (int index = 0; index < phantomTargetCount_; ++index) {
            require(phantomTargets_[index].enemy && !phantomTargets_[index].enemy->IsSniper(),
                "aiming at formation failed to prioritize its three targets");
        }
        require(phantomCooldown_ == kPhantomCooldownFrames, "activation must begin 8-second cooldown");
        const float before = phantomCooldown_;
        RecoverPhantomRaidOnHit(true, true);
        require(phantomCooldown_ == before, "active skill allowed cooldown recovery");
        log("COOLDOWN_8_SECONDS_OK no_self_recharge=1");
    }
    if (frame == 220) {
        require(!IsPhantomRaidActive() && phantomDefeatCount_ == 3 && defeatedEnemyCount_ == 3, "normal triple finish");
        require(enemies_.size() == 1 && enemies_.front()->IsSniper() && !enemies_.front()->IsDead(),
            "formation clear should leave the independently positioned sniper alive");
        log("FORMATION_CHOICE_OK triple_clear=1 sniper_remains=1");
        enemies_.front()->Kill(); // 次の独立ケースへ持ち越さない。報酬には加算しない。
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
        require(bossDefeated_ && phantomDefeatCount_ == 1 &&
            defeatedEnemyCount_ == GetTotalEnemyTargetCount() + 1, "boss clear reward path");
        log("BOSS_SKILL_CLEAR_OK");
        keys[DIK_R] = 0x80;
        retry = true;
    }
    if (frame == 1100) { require(retry, "boss clear not reached"); }
    input_->SetTestFrame(keys, mouse);
    return false;
}
bool GameRuntime::RunBossProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "BOSS_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    if (preview && (frame == 138 || frame == 172 || frame == 366 || frame == 606 || frame == 773)) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
        ImGui::Begin("Boss visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("TEST ONLY: boss windup / recovery / defeat");
        ImGui::Text("Frame %d   Pattern %d   Counter %d", frame, bossAttackPattern_, bossCounterTimer_);
        const bool next = ImGui::Button("NEXT / resume test", { 220.0f, 36.0f });
        ImGui::End();
        if (!next) { input_->SetTestFrame({}, { 640, 320 }); return false; }
        phantomPreviewPaused_ = false;
    }
    ++frame;
    if (frame == 1) {
        require(sound_ && sound_->GetVoiceCount() == 28, "BGM/SFX bank incomplete");
        for (const char* key : { "music_stage", "music_boss", "music_fever" }) {
            require(sound_->PlayLoop(key) && sound_->PlayLoop(key) && sound_->GetLoopCount() == 1,
                "loop not queued or duplicate loop created");
            sound_->Stop(key);
            require(sound_->GetLoopCount() == 0, "loop did not stop");
        }
        require(sound_->GetVoiceCount() == 28 && sound_->GetPlayCount() == 0, "loop changed SFX counter/pool");
        log("LOOPS_OK voices=28 duplicate_safe=1 stop_safe=1 sfx_count_unchanged=1");
        const auto reset = [&]() {
            isGameClear_ = false;
            isGameOver_ = false;
            resultSoundPlayed_ = false;
            score_ = 0;
            feverTimer_ = 0;
            feverGauge_ = 0;
            DebugJumpToStagePhase(3);
            for (auto& effect : hitEffects_) { RecycleHitEffectVisuals(effect); }
            hitEffects_.clear();
            SpawnBossEnemy();
            return enemies_.back().get();
        };
        for (int phase : { 1, 2 }) {
            for (int pattern = 0; pattern < 3; ++pattern) {
                auto* boss = reset();
                require(!boss->IsTargetable(), "boss targetable before arrival");
                for (int step = 0; step < 95; ++step) { boss->Update(railDistance_); }
                require(!boss->IsTargetable(), "boss became targetable before entry completed");
                for (int step = 95; step < 300; ++step) { boss->Update(railDistance_); }
                require(boss->CanShoot(), "boss fixture not ready");
                bossIntroTimer_ = 0;
                bossPhase_ = phase;
                bossAttackSequence_ = pattern;
                bossAttackCooldown_ = 0;
                UpdateBossActions();
                const int windup = bossAttackStepTimer_;
                require(windup >= 38 && enemyBullets_.empty(), "boss fired without a readable windup");
                while (bossAttackStepTimer_ > 14) { UpdateBossActions(); }
                bossAimPoint_.x = 3.75f; // 照準固定後に現在の自機座標で上書きされないことを確認。
                while (bossAttackStepTimer_ > 0) { UpdateBossActions(); }
                require(enemyBullets_.empty() && bossAimPoint_.x == 3.75f, "boss tracked after aim lock");
                UpdateBossActions();
                int guard = 0;
                while (bossAttackStep_ >= 0 && ++guard < 150) { UpdateBossActions(); }
                const size_t expected = pattern == 0 ? (phase == 2 ? 7u : 5u) :
                    pattern == 1 ? (phase == 2 ? 9u : 7u) : (phase == 2 ? 3u : 1u);
                require(bossAttackStep_ == -1 && enemyBullets_.size() == expected && bossAimPoint_.x == 3.75f,
                    "boss pattern count/fixed sweep aim incorrect");
                if (pattern == 2) {
                    const auto p = enemyBullets_.front()->GetTranslate();
                    const auto v = enemyBullets_.front()->GetVelocity();
                    const float t = (p.z - player_->GetTranslate().z - 0.18f) / (railSpeed_ - v.z);
                    require(t > 0 && std::abs(p.x + v.x * t - 3.75f) < 0.01f, "charge ignored fixed aim");
                }
                require(bossCounterTimer_ == (phase == 2 ? 108 : 120) &&
                    bossAttackCooldown_ > bossCounterTimer_, "counter opening too short or overlaps next attack");
                boss->SetBossRecoveryRate(0.5f);
                boss->Update(railDistance_);
                const float x = boss->GetTranslate().x;
                for (int step = 0; step < 40; ++step) { boss->Update(railDistance_); }
                require(std::abs(boss->GetTranslate().x - x) < 0.001f, "boss drifted during recovery");
                const size_t shotCount = enemyBullets_.size();
                for (int step = 0; step < bossCounterDuration_; ++step) { UpdateBossActions(); }
                require(bossCounterTimer_ == 0 && enemyBullets_.size() == shotCount,
                    "boss fired inside counter opening");
                log("PATTERN_OK phase=" + std::to_string(phase) + " pattern=" + std::to_string(pattern) +
                    " windup=" + std::to_string(windup) + " shots=" + std::to_string(shotCount));
            }
        }
        auto* boss = reset();
        for (int step = 0; step < 300; ++step) { boss->Update(railDistance_); }
        boss->Damage(boss->GetMaxHp() / 2);
        FireEnemyBullet(boss->GetAimPosition(), EnemyBulletStyle::BossCharge);
        UpdateBossActions();
        require(bossPhase_ == 2 && bossPhaseTransitionTimer_ > 0 && enemyBullets_.empty(),
            "phase transition retained dangerous bullets");
        log("PHASE_OK hp_half_transition=1 bullets_recycled=1");
        boss->Damage(999);
        OnEnemyDestroyed(*boss, true, false);
        require(isGameClear_ && bossDefeated_ && enemyBullets_.empty() && !resultSoundPlayed_,
            "boss defeat cleanup failed");
        for (int step = 0; step < 45; ++step) { UpdateResultAndSceneObjects(); }
        require(resultSoundPlayed_ && bossDefeatFlashTimer_ == 0, "defeat sequence/result cue not completed");
        require(enemyBulletPoolMisses_ == 0 && hitEffectObjectPoolMisses_ == 0, "boss exhausted fixed pools");
        log("DEFEAT_OK delayed_bursts=3 result_after_bursts=1 pool_misses=0");
        reset();
        cameraShakeTimer_ = 0;
        musicTrack_ = -1;
        musicLevels_.fill(0.0f);
        log("LIVE_VISUAL_BEGIN controlled_fixture=1");
    }
    if (frame == 500) { feverTimer_ = 90; }
    if (frame == 530) {
        require(musicTrack_ == 2 && sound_->GetLoopCount() == 1, "fever crossfade did not settle");
        log("FEVER_MUSIC_OK one_loop=1");
    }
    if (frame == 640) {
        require(musicTrack_ == 1 && sound_->GetLoopCount() == 1, "boss music did not resume");
        log("BOSS_MUSIC_OK one_loop=1");
    }
    if (frame == 750) {
        auto* boss = enemies_.front().get();
        boss->Damage(999); // 撃破映像の固定サンプル。通常通し試験ではこの操作を使わない。
        OnEnemyDestroyed(*boss, true, false);
    }
    if (frame == 840) {
        require(isGameClear_ && sound_->GetLoopCount() == 0 && resultSoundPlayed_, "music remains after clear");
        require(hitEffectObjectPoolMisses_ == 0, "live defeat effect pool exhausted");
        log("PASS clear_music_silent=1 voices=28 pool_misses=0");
        input_->SetTestFrame({}, { 640, 320 });
        return true;
    }
    input_->SetTestFrame({}, { 640, 320 });
    return false;
}

bool GameRuntime::RunChargeShotProbe(const std::string& logPath, bool preview)
{
    static int frame = 0;
    static int impactFrame = -1;
    const auto log = [&](const std::string& message) {
        std::ofstream file(logPath, std::ios::app);
        file << "CHARGE_TEST " << message << '\n';
    };
    const auto require = [&](bool ok, const char* message) {
        if (!ok) { log(std::string("FAIL ") + message); throw std::runtime_error(message); }
    };
    std::array<BYTE, 256> keys{};
    Math::Vector2 mouse = input_->GetMousePosition();
    if (frame > 102 && defeatedEnemyCount_ == 3 && impactFrame < 0) { impactFrame = frame; }
    if (preview && (frame == 100 || (impactFrame >= 0 &&
        (frame == impactFrame || frame == impactFrame + 5 || frame == impactFrame + 12)))) {
        phantomPreviewPaused_ = true;
        ImGui::SetNextWindowPos({ 20.0f, 165.0f }, ImGuiCond_Always);
        ImGui::Begin("Charge visual fixture", nullptr,
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::TextUnformatted("TEST ONLY: charge shot / three small ships + sniper");
        ImGui::Text("Frame: %d   Defeated: %d", frame, defeatedEnemyCount_);
        const bool next = ImGui::Button("NEXT / resume test", { 220.0f, 36.0f });
        ImGui::End();
        if (!next) { input_->SetTestFrame(keys, mouse); return false; }
        phantomPreviewPaused_ = false;
    }
    ++frame;
    if (frame == 1) {
        const auto reset = [&]() {
            DebugJumpToStagePhase(0);
            std::fill(stageEnemyEventTriggered_.begin(), stageEnemyEventTriggered_.end(), true);
            for (auto& effect : hitEffects_) { RecycleHitEffectVisuals(effect); }
            hitEffects_.clear();
            score_ = 0;
            feverGauge_ = 0;
            feverTimer_ = 0;
            playerImpactSlowTimer_ = 0;
            cameraShakeTimer_ = 0;
        };
        const auto spawn = [&](float x, float y, int hp = 3, Enemy::Behavior behavior = Enemy::Behavior::Formation) {
            SpawnStageEnemy(x, y, 44.0f, behavior, Enemy::EntryStyle::TightFormation, hp, 1.0f);
            auto* enemy = enemies_.back().get();
            for (int step = 0; step < 120; ++step) { enemy->Update(railDistance_); }
            require(enemy->IsTargetable(), "fixture not targetable");
            return enemy;
        };
        const auto hit = [&](Enemy& target, bool charged, bool fever = false) {
            auto bullet = AcquireBullet(playerBulletPool_);
            require(bullet != nullptr, "fixture bullet pool exhausted");
            bullet->Initialize(object3dCommon_.get(), bulletModel_, target.GetAimPosition(), {},
                { 1, 1, 1, 1 }, 30, { 0.5f, 0.5f, 1 }, charged ? 1.0f : 0.42f, fever ? 3 : 1);
            bullet->SetFeverShot(fever);
            playerBullets_.push_back(std::move(bullet));
            ++playerShotsFired_;
            CheckBulletEnemyCollisions();
        };
        reset();
        auto* center = spawn(0, 0);
        auto* neighbor = spawn(2.8f, 0);
        hit(*center, false);
        require(center->GetHp() == 2 && neighbor->GetHp() == 3 && playerHitCount_ == 1,
            "normal shot damage or no-splash rule changed");
        log("NORMAL_OK direct=1 no_splash=1");

        reset();
        center = spawn(0, 0, 6);
        neighbor = spawn(2.8f, 0);
        hit(*center, true);
        require(center->GetHp() == 3 && neighbor->IsDead() && defeatedEnemyCount_ == 1 && playerHitCount_ == 1,
            "charge direct damage stacked, splash failed, or accuracy inflated");
        const int score = score_, gauge = feverGauge_;
        CheckBulletEnemyCollisions();
        require(center->GetHp() == 3 && defeatedEnemyCount_ == 1 && score_ == score && feverGauge_ == gauge,
            "spent charge rewarded/damaged twice");
        log("SINGLE_HIT_OK direct=3 splash=3 reward_once=1 accuracy_once=1");

        reset();
        center = spawn(0, 0);
        auto* inside = spawn(3.39f, 0);
        auto* outside = spawn(-3.41f, 0);
        hit(*center, true);
        require(inside->IsDead() && outside->GetHp() == 3, "charge radius boundary wrong");
        log("BOUNDARY_OK radius=3.4 inside=3.39 outside=3.41");

        reset();
        center = spawn(-2.6f, 0);
        neighbor = spawn(0, 0);
        auto* opposite = spawn(2.6f, 0);
        hit(*center, true);
        require(center->IsDead() && neighbor->IsDead() && opposite->GetHp() == 3 && defeatedEnemyCount_ == 2,
            "edge hit chained across formation");
        log("NO_CHAIN_OK edge_hit_kills=2 opposite_ship_alive=1");

        reset();
        center = spawn(0, 0, 6);
        neighbor = spawn(4.0f, 0, 6); // 貫通弾そのものには触れない隣機。
        hit(*center, true, true);
        CheckBulletEnemyCollisions();
        require(center->GetHp() == 2 && neighbor->GetHp() == 6 && !playerBullets_.front()->IsDead() &&
            playerHitCount_ == 1 && hitEffects_.size() == 1,
            "fever damage, piercing or duplicate-hit prevention changed");
        log("FEVER_OK direct=4 piercing_retained=1 no_splash=1 no_repeat=1");

        reset();
        center = spawn(0, 0, 10, Enemy::Behavior::Shield);
        hit(*center, true);
        require(center->GetShieldHp() == 0 && center->GetHp() == 10, "charge bypassed shield");
        reset();
        center = spawn(0, 0, 52, Enemy::Behavior::Boss);
        hit(*center, true);
        require(center->GetHp() == 48, "boss direct damage doubled by own splash");
        bossCounterTimer_ = 30;
        hit(*center, true);
        require(center->GetHp() == 40, "boss counter damage changed");
        log("DEFENSE_OK shield_absorbs=3 boss_direct=4 boss_counter=8");

        reset();
        center = spawn(0, 0);
        neighbor = spawn(0, 0, 6, Enemy::Behavior::Sniper);
        const auto a = center->GetAimPosition(), b = neighbor->GetAimPosition();
        require(std::abs(a.z - b.z) > 3.4f, "depth fixture too close");
        hit(*center, true);
        require(neighbor->GetHp() == 6, "blast hit a distant enemy overlapping on screen");
        log("DEPTH_OK distant_ship_undamaged=1");

        reset();
        center = spawn(0, 0);
        neighbor = spawn(2.8f, 0);
        neighbor->Kill();
        const auto p = center->GetAimPosition();
        SpawnStageEnemy(p.x, p.y, p.z - railDistance_, Enemy::Behavior::Formation,
            Enemy::EntryStyle::Direct, 3, 1.0f);
        auto* entering = enemies_.back().get();
        require(!entering->IsTargetable(), "entry fixture already targetable");
        hit(*center, true);
        require(entering->GetHp() == 3 && defeatedEnemyCount_ == 1, "blast hit entry/dead enemy");
        log("LIFETIME_OK entry_ignored=1 destroyed_ignored=1");

        reset();
        // ここからは手動の命中配置ではなく、本編と同じ動く編隊へ実際に一発撃つ。
        SpawnStageEnemy(-4.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(-2.0f, 0.5f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(0.6f, -0.6f, 44.0f, Enemy::Behavior::Formation, Enemy::EntryStyle::TightFormation, 3, 1.0f);
        SpawnStageEnemy(5.3f, 2.0f, 48.0f, Enemy::Behavior::Sniper, Enemy::EntryStyle::PopShooter, 6, 1.18f);
        log("LIVE_FORMATION_BEGIN normal_input=1");
    }
    if (frame == 101 || frame == 102) {
        Math::Vector2 screen{};
        require(TryProjectToScreen((*std::next(enemies_.begin()))->GetAimPosition(), screen), "center offscreen");
        const ImVec2 origin = ImGui::GetMainViewport()->Pos;
        mouse = { screen.x - origin.x, screen.y - origin.y };
    }
    if (frame == 102) {
        require(chargeTimer_ >= chargeShotThreshold_, "normal waiting did not charge shot");
        require(lockedEnemy_ == (*std::next(enemies_.begin())).get() && isReticleOnTarget_,
            "fixture did not aim at formation center");
        keys[DIK_SPACE] = 0x80;
    }
    if (frame == 190) {
        log("LIVE_RESULT kills=" + std::to_string(defeatedEnemyCount_) + " shots=" +
            std::to_string(playerShotsFired_) + " hits=" + std::to_string(playerHitCount_));
        require(defeatedEnemyCount_ == 3 && playerShotsFired_ == 1 && playerHitCount_ == 1,
            "real charged projectile failed to clear moving three-ship formation");
        require(enemies_.size() == 1 && enemies_.front()->IsSniper() && enemies_.front()->GetHp() == 6,
            "real splash reached distant sniper");
        require(playerBulletPoolMisses_ == 0 && hitEffectObjectPoolMisses_ == 0, "effect/bullet pool exhausted");
        log("PASS real_projectile_kills=3 sniper_hp=6 shots=1 hits=1 pool_misses=0");
        input_->SetTestFrame(keys, mouse);
        return true;
    }
    input_->SetTestFrame(keys, mouse);
    return false;
}
#endif
