#pragma once
#include "engine/base/Math.h"

struct Player {
    Math::Vector3 pos{ 0,0,0 };
    Math::Vector3 vel{ 0,0,0 };
    static constexpr Math::Vector3 Size{ 1.0f, 1.8f, 0.0f };

    bool facingRight = true;

    // 地上/壁
    bool onGround = false;
    bool onWallLeft = false;
    bool onWallRight = false;
    float wallSlideSpeed = -3.0f;
    float wallKickLock = 0.0f;

    // 空中ダッシュ
    bool  isDashing = false;
    bool  canAirDash = true;
    float dashSpeed = 18.0f;
    float dashTime = 0.18f;
    float dashTimer = 0.0f;

    // 二段ジャンプ
    bool canDoubleJump = false;

    // チャージ
    float charge = 0.0f;
    float maxChargeTime = 1.0f;
    bool  isCharging = false;

    // HP/無敵（必要なら）
    float hp = 100.0f;
    float maxHp = 100.0f;
    bool  invincible = false;
    float invincibleTime = 0.0f;
    float invincibleDuration = 1.0f;
    float blinkTimer = 0.0f;

    // ノックバック（必要なら）
    float knockbackPower = 8.0f;
    float knockbackUp = 5.0f;
};
