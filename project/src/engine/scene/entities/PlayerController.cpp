#include "engine/scene/entities/PlayerController.h"
#include "engine/scene/world/Stage.h"
#include "engine/io/Input.h"
#include <cmath>

static inline float Clamp(float v, float a, float b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

extern Math::Vector3 kPlayerSize;
static constexpr float TILE = Stage::kTileSize;

void PlayerController::Update(Player& P, const Stage& stage, const Input& input, float dt)
{
    const float runSpeed = 6.0f;
    const float gravity = -22.0f;
    const float jumpVel = 10.0f;

    // ------------------------
    // 入力
    // ------------------------
    float moveX = 0.0f;
    if (input.PushKey(DIK_LEFT) || input.PushKey(DIK_A)) moveX -= 1.0f;
    if (input.PushKey(DIK_RIGHT) || input.PushKey(DIK_D)) moveX += 1.0f;

    if (moveX != 0.0f) {
        P.facingRight = (moveX > 0);
    }

    if (P.wallKickLock > 0.0f) {
        P.wallKickLock -= dt;
        if (P.wallKickLock < 0.0f) P.wallKickLock = 0.0f;
    }

    // ダッシュ中は横入力無視 / 壁キックロック中も無視
    if (!P.isDashing && P.wallKickLock <= 0.0f) {
        P.vel.x = moveX * runSpeed;
    }

    // ------------------------
    // 重力
    // ------------------------
    P.vel.y += gravity * dt;

    P.onWallLeft = false;
    P.onWallRight = false;

    MoveX(P, stage, dt);
    MoveY(P, stage, dt);

    if (P.onGround) {
        P.canDoubleJump = true;
        P.canAirDash = true;
    }

    // ------------------------
    // 空中ダッシュ
    // ------------------------
    if (!P.onGround && !P.isDashing && P.canAirDash && input.TriggerKey(DIK_LSHIFT)) {
        P.isDashing = true;
        P.dashTimer = P.dashTime;

        float dir = (moveX == 0.0f ? (P.facingRight ? 1.0f : -1.0f) : moveX);
        P.vel.x = dir * P.dashSpeed;
        P.vel.y = 0.0f;

        P.canAirDash = false;
    }

    if (P.isDashing) {
        P.dashTimer -= dt;
        if (P.dashTimer <= 0.0f) {
            P.isDashing = false;
        }
    }

    // ------------------------
    // ジャンプ（通常/壁/二段）
    // ------------------------
    if (input.TriggerKey(DIK_SPACE)) {
        if (P.onGround) {
            P.vel.y = jumpVel;
            P.canDoubleJump = true;
        } else if (P.onWallLeft) {
            P.vel.x = +14.0f;
            P.vel.y = +12.0f;

            P.facingRight = true;
            P.canDoubleJump = true;

            P.wallKickLock = 0.15f;
            P.onWallLeft = P.onWallRight = false;
        } else if (P.onWallRight) {
            P.vel.x = -14.0f;
            P.vel.y = +12.0f;

            P.facingRight = false;
            P.canDoubleJump = true;

            P.wallKickLock = 0.15f;
            P.onWallLeft = P.onWallRight = false;
        } else if (P.canDoubleJump) {
            P.vel.y = jumpVel;
            P.canDoubleJump = false;
        }
    }

    // 壁スライド
    if (!P.onGround && (P.onWallLeft || P.onWallRight)) {
        if (P.vel.y < P.wallSlideSpeed) {
            P.vel.y = P.wallSlideSpeed;
        }
    }

    // ------------------------
    // チャージ（Z）
    // ------------------------
    if (input.PushKey(DIK_Z)) {
        isCharging_ = true;

        P.isCharging = true;
        P.charge += dt / P.maxChargeTime;
        P.charge = Clamp(P.charge, 0.0f, 1.0f);

        charge01_ = P.charge;
    } else {
        if (P.isCharging) {
            float front = (P.facingRight ? 1.0f : -1.0f);
            float offset = 0.6f;

            float cx = P.pos.x + kPlayerSize.x * 0.5f + front * offset;
            float cy = P.pos.y + kPlayerSize.y * 0.5f;

            float power = 0.4f + P.charge * 1.6f;
            float speed = 14.0f + P.charge * 10.0f;
            float damage = 3.0f + P.charge * 10.0f;

            shotRequested_ = true;
            shotPos_ = { cx - power * 0.5f, cy - power * 0.5f, 0.0f };
            shotVel_ = { front * speed, 0.0f, 0.0f };
            shotPower_ = power;
            shotDamage_ = damage;
            shotLife_ = 1.0f + P.charge * 0.8f;
        }

        isCharging_ = false;
        charge01_ = 0.0f;

        P.isCharging = false;
        P.charge = 0.0f;
    }
}

void PlayerController::MoveX(Player& P, const Stage& stage, float dt)
{
    const bool skipWallCollision = (P.wallKickLock > 0.0f);
    float newX = P.pos.x + P.vel.x * dt;

    if (!skipWallCollision) {
        // 高さ方向に複数点チェック（足元〜頭）
        const float y0 = P.pos.y + 0.1f;
        const float y1 = P.pos.y + kPlayerSize.y - 0.1f;

        if (P.vel.x > 0.0f) {
            const float right = newX + kPlayerSize.x;

            // 右側の壁を、上から下までサンプリング
            for (float wy = y0; wy <= y1; wy += 0.9f) {
                if (stage.IsSolidAtWorld(right, wy)) {
                    // 衝突したタイル境界まで戻す
                    const int tileX = Stage::WorldToTileX(right);
                    newX = tileX * TILE - kPlayerSize.x;
                    P.vel.x = 0.0f;
                    P.onWallRight = true;
                    break;
                }
            }
        } else if (P.vel.x < 0.0f) {
            const float left = newX;

            for (float wy = y0; wy <= y1; wy += 0.9f) {
                if (stage.IsSolidAtWorld(left, wy)) {
                    const int tileX = Stage::WorldToTileX(left);
                    newX = (tileX + 1) * TILE;
                    P.vel.x = 0.0f;
                    P.onWallLeft = true;
                    break;
                }
            }
        }
    }

    P.pos.x = newX;
}

void PlayerController::MoveY(Player& P, const Stage& stage, float dt)
{
    P.onGround = false;
    float newY = P.pos.y + P.vel.y * dt;

    const float x0 = P.pos.x + 0.1f;
    const float x1 = P.pos.x + kPlayerSize.x - 0.1f;

    if (P.vel.y > 0.0f) {
        const float top = newY + kPlayerSize.y;

        for (float wx = x0; wx <= x1; wx += 0.9f) {
            if (stage.IsSolidAtWorld(wx, top)) {
                const int tileY = Stage::WorldToTileYWorld(top);
                newY = tileY * TILE - kPlayerSize.y;
                P.vel.y = 0.0f;
                break;
            }
        }
    } else {
        const float bottom = newY;

        for (float wx = x0; wx <= x1; wx += 0.9f) {
            if (stage.IsSolidAtWorld(wx, bottom)) {
                const int tileY = Stage::WorldToTileYWorld(bottom);
                newY = (tileY + 1) * TILE;
                P.vel.y = 0.0f;
                P.onGround = true;
                break;
            }
        }
    }

    P.pos.y = newY;
}


bool PlayerController::ConsumeShotRequest(Math::Vector3& outPos, Math::Vector3& outVel,
    float& outPower, float& outDamage, float& outLife)
{
    if (!shotRequested_) return false;

    outPos = shotPos_;
    outVel = shotVel_;
    outPower = shotPower_;
    outDamage = shotDamage_;
    outLife = shotLife_;

    shotRequested_ = false;
    return true;
}
