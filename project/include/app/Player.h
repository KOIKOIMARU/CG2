#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"

#include <memory>

class Input;
class Model;
class Object3dCommon;

class Player {
public:
    void Initialize(Object3dCommon* object3dCommon, Model* model);
    void Update(Input* input);
    void Draw();
    void SetRailZ(float z);

    const Math::Vector3& GetTranslate() const { return translate_; }
    int GetHp() const { return hp_; }
    float GetRadius() const { return collisionRadius_; }
    bool IsDead() const { return hp_ <= 0; }
    bool IsInvincible() const { return invincibleTimer_ > 0; }
    void Damage(int amount);

private:
    void UpdateObjectTransform();

private:
    std::unique_ptr<Object3d> object_;
    Math::Vector3 translate_{ 0.0f, 0.0f, 0.0f };
    Math::Vector3 objectScale_{ 0.62f, 0.62f, 0.62f };
    Math::Vector3 objectRotate_{ 0.0f, 0.0f, 0.0f };
    Math::Vector3 modelLocalCenterOffset_{ 0.0f, 0.0f, 0.0f };
    float moveSpeed_ = 0.18f;
    float collisionRadius_ = 0.7f;
    int dodgeTimer_ = 0;
    int dodgeCooldownTimer_ = 0;
    int invincibleTimer_ = 0;
    int dodgeDirection_ = 1;
    int lastHorizontalDirection_ = 1;
    int hp_ = 100;
};
