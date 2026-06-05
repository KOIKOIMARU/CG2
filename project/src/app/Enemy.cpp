#include "app/Enemy.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.1415926535f;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

} // namespace

void Enemy::Initialize(
    Object3dCommon* object3dCommon,
    Model* model,
    const Math::Vector3& position,
    Behavior behavior)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    behavior_ = behavior;

    switch (behavior_) {
    case Behavior::Swoop:
        object_->SetScale({ 0.75f, 0.75f, 0.75f });
        object_->SetColor({ 0.95f, 0.35f, 1.0f, 1.0f });
        horizontalAmplitude_ = 0.75f;
        verticalAmplitude_ = 0.35f;
        collisionRadius_ = 0.7f;
        break;
    case Behavior::StrafeShooter:
        object_->SetScale({ 1.1f, 1.1f, 1.1f });
        object_->SetColor({ 1.0f, 0.65f, 0.2f, 1.0f });
        horizontalAmplitude_ = 1.9f;
        verticalAmplitude_ = 0.25f;
        collisionRadius_ = 0.9f;
        break;
    case Behavior::Formation:
    default:
        object_->SetScale({ 0.9f, 0.9f, 0.9f });
        object_->SetColor({ 1.0f, 0.25f, 0.25f, 1.0f });
        break;
    }

    baseTranslate_ = position;
    translate_ = position;
    phase_ = position.x * 0.75f + position.y * 1.35f;
    object_->SetTranslate(translate_);
    object_->Update();
}

void Enemy::Update(float railDistance)
{
    if (isDead_ || !object_) {
        return;
    }

    moveTimer_ += 0.06f;
    const float distanceAhead = baseTranslate_.z - railDistance;

    switch (behavior_) {
    case Behavior::Swoop: {
        const float passRate = Clamp01((30.0f - distanceAhead) / 24.0f);
        const float curve = std::sin(passRate * kPi);
        translate_.x =
            baseTranslate_.x * (1.0f - passRate) +
            (-baseTranslate_.x * 0.35f) * passRate +
            std::sin(moveTimer_ * 2.8f + phase_) * horizontalAmplitude_;
        translate_.y =
            baseTranslate_.y +
            curve * 1.7f +
            std::cos(moveTimer_ * 2.1f + phase_) * verticalAmplitude_;
        translate_.z = baseTranslate_.z - curve * 4.5f;
        break;
    }
    case Behavior::StrafeShooter: {
        const float aimRate = Clamp01((24.0f - distanceAhead) / 14.0f);
        translate_.x =
            baseTranslate_.x +
            std::sin(moveTimer_ * 1.35f + phase_) * horizontalAmplitude_ *
            (0.5f + aimRate * 0.5f);
        translate_.y =
            baseTranslate_.y * (1.0f - aimRate * 0.35f) +
            std::cos(moveTimer_ * 1.6f + phase_) * verticalAmplitude_;
        translate_.z = baseTranslate_.z - aimRate * 1.5f;
        break;
    }
    case Behavior::Formation:
    default:
        translate_.x =
            baseTranslate_.x +
            std::sin(moveTimer_ * 1.8f + phase_) * horizontalAmplitude_;
        translate_.y =
            baseTranslate_.y +
            std::cos(moveTimer_ * 1.25f + phase_) * verticalAmplitude_;
        translate_.z = baseTranslate_.z;
        break;
    }

    if (translate_.z < railDistance - 8.0f) {
        isDead_ = true;
        return;
    }

    object_->SetTranslate(translate_);
    object_->Update();
}

void Enemy::Draw()
{
    if (!isDead_ && object_) {
        object_->Draw();
    }
}
