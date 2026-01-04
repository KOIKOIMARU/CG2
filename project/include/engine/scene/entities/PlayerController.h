#pragma once
#include "Player.h"
#include "engine/base/Math.h"

class Stage;
class Input;

class PlayerController {
public:
    void Update(Player& P, const Stage& stage, const Input& input, float dt);

    bool ConsumeShotRequest(Math::Vector3& outPos, Math::Vector3& outVel,
        float& outPower, float& outDamage, float& outLife);

private:
    void MoveX(Player& P, const Stage& stage, float dt);
    void MoveY(Player& P, const Stage& stage, float dt);

    bool shotRequested_ = false;
    Math::Vector3 shotPos_{};
    Math::Vector3 shotVel_{};

    float shotPower_ = 0.0f;
    float shotDamage_ = 0.0f;
    float shotLife_ = 0.0f;

    bool  isCharging_ = false;
    float charge01_ = 0.0f;
};
