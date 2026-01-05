#pragma once
#include "engine/base/Math.h"

class Stage;
struct Player;

class Boss {
public:
    enum class State {
        Idle,
        Approach,
        Attack,
        DashPrep,
        Dash
    };

public:
    // ★ Controller統合版
    void Update(const Player& player, const Stage& stage, float dt);

    void Damage(float dmg);

    bool IsAlive() const { return alive_; }
    const Math::Vector3& GetPos() const { return pos_; }
    float GetSizeX() const { return sizeX_; }
    float GetSizeY() const { return sizeY_; }

    // 状態（今のまま publicでもOK。あとで整理してもいい）
    Math::Vector3 pos_{ 10.0f, 1.0f, 0.0f };
    Math::Vector3 vel_{ 0,0,0 };

    float sizeX_ = 1.8f;
    float sizeY_ = 1.8f;

    float hp_ = 120.0f;
    float maxHp_ = 120.0f;
    bool  alive_ = true;

    bool facingRight_ = true;
    State state_ = State::Idle;

    float walkSpeed_ = 1.5f;
    float attackRange_ = 2.0f;
    float dashRange_ = 15.0f;
    float keepDistance_ = 6.0f;

    float attackCooldown_ = 0.0f;
    float dashInstantSpeed_ = 0.0f;
};
