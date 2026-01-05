#pragma once
#include "engine/base/Math.h"

class Stage;
class Input;

struct Player {

    // =========================
    // 公開API
    // =========================
    void Update(const Stage& stage, const Input& input, float dt);

    // 弾生成要求を取り出す（旧 PlayerController::ConsumeShotRequest）
    bool ConsumeShotRequest(Math::Vector3& outPos, Math::Vector3& outVel,
        float& outPower, float& outDamage, float& outLife);

    // =========================
    // 状態
    // =========================
    Math::Vector3 pos{ 0,0,0 };
    Math::Vector3 vel{ 0,0,0 };

    bool facingRight = true;

    bool onGround = false;
    bool onWallLeft = false;
    bool onWallRight = false;
    float wallSlideSpeed = -3.0f;
    float wallKickLock = 0.0f;

    bool  isDashing = false;
    bool  canAirDash = true;
    float dashSpeed = 18.0f;
    float dashTime = 0.18f;
    float dashTimer = 0.0f;

    bool canDoubleJump = false;

    float charge = 0.0f;
    float maxChargeTime = 1.0f;
    bool  isCharging = false;

    float hp = 100.0f;
    float maxHp = 100.0f;

    // =========================
// ダメージ/無敵/点滅（GameSceneで使用）
// =========================
    bool  invincible = false;
    float invincibleTime = 0.0f;
    float invincibleDuration = 1.0f;
    float blinkTimer = 0.0f;

    // ノックバック（GameSceneで使用）
    float knockbackPower = 8.0f;
    float knockbackUp = 5.0f;


    // =========================
    // サイズ（今あなたは global kPlayerSize を使ってるのでそれは残すなら残す）
    // できればここに統一するのが綺麗
    // =========================
    static constexpr Math::Vector3 Size{ 1.0f, 1.8f, 0.0f };

private:
    void MoveX(const Stage& stage, float dt);
    void MoveY(const Stage& stage, float dt);

    // ---- 旧 PlayerController の内部状態 ----
    bool shotRequested_ = false;
    Math::Vector3 shotPos_{};
    Math::Vector3 shotVel_{};
    float shotPower_ = 0.0f;
    float shotDamage_ = 0.0f;
    float shotLife_ = 0.0f;

    bool  isChargingInternal_ = false;
    float charge01_ = 0.0f;
};
