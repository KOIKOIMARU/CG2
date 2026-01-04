#include "engine/scene/entities/BossController.h"
#include "engine/scene/world/Stage.h"
#include "engine/scene/entities/Player.h"
#include <cmath>

static constexpr float TILE = Stage::kTileSize;

void BossController::Update(Boss& B, const Player& P, const Stage& stage, float dt)
{
    if (!B.alive_) return;

    float dx = P.pos.x - B.pos_.x;
    float dist = std::fabs(dx);
    B.facingRight_ = (dx > 0);

    if (B.attackCooldown_ > 0.0f) {
        B.attackCooldown_ -= dt;
    }

    // ================= 状態遷移 =================
    switch (B.state_)
    {
    case Boss::State::Idle:
        if (dist < 200.0f) {
            B.state_ = Boss::State::Approach;
        }
        break;

    case Boss::State::Approach:
        B.vel_.x = (B.facingRight_ ? B.walkSpeed_ : -B.walkSpeed_);

        if (dist < B.attackRange_ && B.attackCooldown_ <= 0.0f) {
            B.state_ = Boss::State::Attack;
        } else if (dist < B.dashRange_ && B.attackCooldown_ <= 0.0f) {
            B.state_ = Boss::State::DashPrep;
        }
        break;

    case Boss::State::Attack:
        B.vel_.x = 0.0f;
        B.attackCooldown_ = 1.2f;
        B.state_ = Boss::State::Idle;
        break;

    case Boss::State::DashPrep:
        B.vel_.x = 0.0f;
        B.attackCooldown_ = 0.1f;

        B.dashInstantSpeed_ = std::fabs(dx) * 10.0f;
        B.state_ = Boss::State::Dash;
        break;

    case Boss::State::Dash:
        B.vel_.x = (B.facingRight_ ? B.dashInstantSpeed_ : -B.dashInstantSpeed_);

        if (dist < B.attackRange_) {
            B.vel_.x = 0.0f;
            B.attackCooldown_ = 5.0f;
            B.state_ = Boss::State::Idle;
        } else if (dist > B.dashRange_ * 2.0f) {
            B.vel_.x = 0.0f;
            B.state_ = Boss::State::Idle;
        }
        break;
    }

    // ================= 重力 =================
    const float gravity = -22.0f;
    B.vel_.y += gravity * dt;

    // ================= Y衝突（床のみ） =================
    float newY = B.pos_.y + B.vel_.y * dt;
    float bottom = newY;

    int tileY = (int)std::floor(bottom / TILE);

    float left = B.pos_.x;
    float right = B.pos_.x + B.sizeX_;

    int tileXLeft = (int)std::floor(left / TILE);
    int tileXRight = (int)std::floor(right / TILE);

    for (int tx = tileXLeft; tx <= tileXRight; ++tx) {
        int mapY = Stage::TileYWorldToMapY(tileY);
        if (stage.IsSolidTileByIndex(tx, mapY)) {
            newY = (tileY + 1) * TILE;
            B.vel_.y = 0.0f;
            break;
        }
    }

    B.pos_.y = newY;

    // ================= X更新 =================
    B.pos_.x += B.vel_.x * dt;
}
