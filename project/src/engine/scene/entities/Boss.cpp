#include "engine/scene/entities/Boss.h"
#include "engine/scene/world/Stage.h"
#include "engine/scene/entities/Player.h"
#include <cmath>

static constexpr float TILE = Stage::kTileSize;

void Boss::Update(const Player& P, const Stage& stage, float dt)
{
    if (!alive_) return;

    float dx = P.pos.x - pos_.x;
    float dist = std::fabs(dx);
    facingRight_ = (dx > 0);

    if (attackCooldown_ > 0.0f) {
        attackCooldown_ -= dt;
        if (attackCooldown_ < 0.0f) attackCooldown_ = 0.0f;
    }

    // ================= 状態遷移 =================
    switch (state_)
    {
    case State::Idle:
        if (dist < 200.0f) {
            state_ = State::Approach;
        }
        break;

    case State::Approach:
        vel_.x = (facingRight_ ? walkSpeed_ : -walkSpeed_);

        // ※今のコードは「attackRange優先」になってる
        if (dist < attackRange_ && attackCooldown_ <= 0.0f) {
            state_ = State::Attack;
        } else if (dist < dashRange_ && attackCooldown_ <= 0.0f) {
            state_ = State::DashPrep;
        }
        break;

    case State::Attack:
        vel_.x = 0.0f;
        attackCooldown_ = 1.2f;
        state_ = State::Idle;
        break;

    case State::DashPrep:
        vel_.x = 0.0f;
        attackCooldown_ = 0.1f;

        dashInstantSpeed_ = std::fabs(dx) * 10.0f;
        state_ = State::Dash;
        break;

    case State::Dash:
        vel_.x = (facingRight_ ? dashInstantSpeed_ : -dashInstantSpeed_);

        if (dist < attackRange_) {
            vel_.x = 0.0f;
            attackCooldown_ = 5.0f;
            state_ = State::Idle;
        } else if (dist > dashRange_ * 2.0f) {
            vel_.x = 0.0f;
            state_ = State::Idle;
        }
        break;
    }

    // ================= 重力 =================
    const float gravity = -22.0f;
    vel_.y += gravity * dt;

    // ================= Y衝突（床のみ） =================
    float newY = pos_.y + vel_.y * dt;
    float bottom = newY;

    int tileY = (int)std::floor(bottom / TILE);

    float left = pos_.x;
    float right = pos_.x + sizeX_;

    int tileXLeft = (int)std::floor(left / TILE);
    int tileXRight = (int)std::floor(right / TILE);

    for (int tx = tileXLeft; tx <= tileXRight; ++tx) {
        int mapY = Stage::TileYWorldToMapY(tileY);
        if (stage.IsSolidTileByIndex(tx, mapY)) {
            newY = (tileY + 1) * TILE;
            vel_.y = 0.0f;
            break;
        }
    }

    pos_.y = newY;

    // ================= X更新 =================
    pos_.x += vel_.x * dt;
}

void Boss::Damage(float dmg)
{
    if (!alive_) return;

    hp_ -= dmg;
    if (hp_ <= 0.0f) {
        hp_ = 0.0f;
        alive_ = false;
    }
}
