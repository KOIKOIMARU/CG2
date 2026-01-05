#include "engine/scene/entities/Player.h"
#include "engine/scene/world/Stage.h"
#include "engine/io/Input.h"

static inline float Clamp(float v, float a, float b) {
    if (v < a) return a;
    if (v > b) return b;
    return v;
}

extern Math::Vector3 kPlayerSize;
static constexpr float TILE = Stage::kTileSize;

Math::Vector3 kPlayerSize = { 0.8f, 1.6f, 1.0f };


void Player::Update(const Stage& stage, const Input& input, float dt)
{
    const float runSpeed = 6.0f;
    const float gravity = -22.0f;
    const float jumpVel = 10.0f;

    float moveX = 0.0f;
    if (input.PushKey(DIK_LEFT) || input.PushKey(DIK_A)) moveX -= 1.0f;
    if (input.PushKey(DIK_RIGHT) || input.PushKey(DIK_D)) moveX += 1.0f;

    if (moveX != 0.0f) {
        facingRight = (moveX > 0);
    }

    if (wallKickLock > 0.0f) {
        wallKickLock -= dt;
        if (wallKickLock < 0.0f) wallKickLock = 0.0f;
    }

    if (!isDashing && wallKickLock <= 0.0f) {
        vel.x = moveX * runSpeed;
    }

    vel.y += gravity * dt;

    onWallLeft = false;
    onWallRight = false;

    MoveX(stage, dt);
    MoveY(stage, dt);

    if (onGround) {
        canDoubleJump = true;
        canAirDash = true;
    }

    // 空中ダッシュ
    if (!onGround && !isDashing && canAirDash && input.TriggerKey(DIK_LSHIFT)) {
        isDashing = true;
        dashTimer = dashTime;

        float dir = (moveX == 0.0f ? (facingRight ? 1.0f : -1.0f) : moveX);
        vel.x = dir * dashSpeed;
        vel.y = 0.0f;

        canAirDash = false;
    }

    if (isDashing) {
        dashTimer -= dt;
        if (dashTimer <= 0.0f) {
            isDashing = false;
        }
    }

    // ジャンプ
    if (input.TriggerKey(DIK_SPACE)) {
        if (onGround) {
            vel.y = jumpVel;
            canDoubleJump = true;
        } else if (onWallLeft) {
            vel.x = +14.0f;
            vel.y = +12.0f;

            facingRight = true;
            canDoubleJump = true;

            wallKickLock = 0.15f;
            onWallLeft = onWallRight = false;
        } else if (onWallRight) {
            vel.x = -14.0f;
            vel.y = +12.0f;

            facingRight = false;
            canDoubleJump = true;

            wallKickLock = 0.15f;
            onWallLeft = onWallRight = false;
        } else if (canDoubleJump) {
            vel.y = jumpVel;
            canDoubleJump = false;
        }
    }

    // 壁スライド
    if (!onGround && (onWallLeft || onWallRight)) {
        if (vel.y < wallSlideSpeed) {
            vel.y = wallSlideSpeed;
        }
    }

    // チャージ（Z）
    if (input.PushKey(DIK_Z)) {
        isChargingInternal_ = true;

        isCharging = true;
        charge += dt / maxChargeTime;
        charge = Clamp(charge, 0.0f, 1.0f);

        charge01_ = charge;
    } else {
        if (isCharging) {
            float front = (facingRight ? 1.0f : -1.0f);
            float offset = 0.6f;

            float cx = pos.x + kPlayerSize.x * 0.5f + front * offset;
            float cy = pos.y + kPlayerSize.y * 0.5f;

            float power  = 0.4f + charge * 1.6f;
            float speed  = 14.0f + charge * 10.0f;
            float damage = 3.0f + charge * 10.0f;

            shotRequested_ = true;
            shotPos_ = { cx - power * 0.5f, cy - power * 0.5f, 0.0f };
            shotVel_ = { front * speed, 0.0f, 0.0f };
            shotPower_ = power;
            shotDamage_ = damage;
            shotLife_ = 1.0f + charge * 0.8f;
        }

        isChargingInternal_ = false;
        charge01_ = 0.0f;

        isCharging = false;
        charge = 0.0f;
    }
}

void Player::MoveX(const Stage& stage, float dt)
{
    const bool skipWallCollision = (wallKickLock > 0.0f);
    float newX = pos.x + vel.x * dt;

    if (!skipWallCollision) {
        const float y0 = pos.y + 0.1f;
        const float y1 = pos.y + kPlayerSize.y - 0.1f;

        if (vel.x > 0.0f) {
            const float right = newX + kPlayerSize.x;
            for (float wy = y0; wy <= y1; wy += 0.9f) {
                if (stage.IsSolidAtWorld(right, wy)) {
                    const int tileX = Stage::WorldToTileX(right);
                    newX = tileX * TILE - kPlayerSize.x;
                    vel.x = 0.0f;
                    onWallRight = true;
                    break;
                }
            }
        } else if (vel.x < 0.0f) {
            const float left = newX;
            for (float wy = y0; wy <= y1; wy += 0.9f) {
                if (stage.IsSolidAtWorld(left, wy)) {
                    const int tileX = Stage::WorldToTileX(left);
                    newX = (tileX + 1) * TILE;
                    vel.x = 0.0f;
                    onWallLeft = true;
                    break;
                }
            }
        }
    }

    pos.x = newX;
}

void Player::MoveY(const Stage& stage, float dt)
{
    onGround = false;
    float newY = pos.y + vel.y * dt;

    const float x0 = pos.x + 0.1f;
    const float x1 = pos.x + kPlayerSize.x - 0.1f;

    if (vel.y > 0.0f) {
        const float top = newY + kPlayerSize.y;
        for (float wx = x0; wx <= x1; wx += 0.9f) {
            if (stage.IsSolidAtWorld(wx, top)) {
                const int tileY = Stage::WorldToTileYWorld(top);
                newY = tileY * TILE - kPlayerSize.y;
                vel.y = 0.0f;
                break;
            }
        }
    } else {
        const float bottom = newY;
        for (float wx = x0; wx <= x1; wx += 0.9f) {
            if (stage.IsSolidAtWorld(wx, bottom)) {
                const int tileY = Stage::WorldToTileYWorld(bottom);
                newY = (tileY + 1) * TILE;
                vel.y = 0.0f;
                onGround = true;
                break;
            }
        }
    }

    pos.y = newY;
}

bool Player::ConsumeShotRequest(Math::Vector3& outPos, Math::Vector3& outVel,
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
