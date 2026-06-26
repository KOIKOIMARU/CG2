#include "app/Enemy.h"

#include "engine/3d/Model.h"

#include <algorithm>
#include <cmath>

namespace {

constexpr float kPi = 3.1415926535f;

float Clamp01(float value)
{
    return std::clamp(value, 0.0f, 1.0f);
}

float Lerp(float start, float end, float rate)
{
    return start + (end - start) * rate;
}

float SignNonZero(float value)
{
    return value < 0.0f ? -1.0f : 1.0f;
}

struct ModelAimBounds {
    Math::Vector3 center{};
    float radius = 0.0f;
    bool isValid = false;
};

ModelAimBounds CalculateModelAimBounds(const Model* model)
{
    if (!model || model->GetVertices().empty()) {
        return {};
    }

    const auto& vertices = model->GetVertices();
    Math::Vector3 minPosition{
        vertices.front().position.x,
        vertices.front().position.y,
        vertices.front().position.z
    };
    Math::Vector3 maxPosition = minPosition;

    for (const auto& vertex : vertices) {
        minPosition.x = (std::min)(minPosition.x, vertex.position.x);
        minPosition.y = (std::min)(minPosition.y, vertex.position.y);
        minPosition.z = (std::min)(minPosition.z, vertex.position.z);
        maxPosition.x = (std::max)(maxPosition.x, vertex.position.x);
        maxPosition.y = (std::max)(maxPosition.y, vertex.position.y);
        maxPosition.z = (std::max)(maxPosition.z, vertex.position.z);
    }

    ModelAimBounds bounds{};
    bounds.center = {
        (minPosition.x + maxPosition.x) * 0.5f,
        (minPosition.y + maxPosition.y) * 0.5f,
        (minPosition.z + maxPosition.z) * 0.5f
    };

    float radiusSq = 0.0f;
    for (const auto& vertex : vertices) {
        const float dx = vertex.position.x - bounds.center.x;
        const float dy = vertex.position.y - bounds.center.y;
        const float dz = vertex.position.z - bounds.center.z;
        radiusSq = (std::max)(radiusSq, dx * dx + dy * dy + dz * dz);
    }

    bounds.radius = std::sqrt(radiusSq);
    bounds.isValid = true;
    return bounds;
}

} // namespace

void Enemy::Initialize(
    Object3dCommon* object3dCommon,
    Model* model,
    const Math::Vector3& position,
    Behavior behavior,
    EntryStyle entryStyle)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    behavior_ = behavior;
    entryStyle_ = entryStyle;
    lifeState_ = LifeState::Alive;
    age_ = 0;
    hitFlashTimer_ = 0;
    moveTimer_ = 0.0f;

    switch (behavior_) {
    case Behavior::Swoop:
        baseScale_ = { 1.18f, 1.18f, 1.18f };
        baseColor_ = { 0.95f, 0.35f, 1.0f, 1.0f };
        maxHp_ = 2;
        horizontalAmplitude_ = 0.75f;
        verticalAmplitude_ = 0.35f;
        collisionRadius_ = 0.85f;
        break;
    case Behavior::StrafeShooter:
        baseScale_ = { 1.62f, 1.62f, 1.62f };
        baseColor_ = { 1.0f, 0.65f, 0.2f, 1.0f };
        maxHp_ = 3;
        horizontalAmplitude_ = 1.9f;
        verticalAmplitude_ = 0.25f;
        collisionRadius_ = 1.05f;
        break;
    case Behavior::Formation:
    default:
        baseScale_ = { 1.30f, 1.30f, 1.30f };
        baseColor_ = { 1.0f, 0.25f, 0.25f, 1.0f };
        maxHp_ = 2;
        collisionRadius_ = 0.9f;
        break;
    }
    hp_ = maxHp_;

    const ModelAimBounds aimBounds = CalculateModelAimBounds(model);
    if (aimBounds.isValid) {
        const float maxScale =
            (std::max)(baseScale_.x, (std::max)(baseScale_.y, baseScale_.z));
        aimLocalCenter_ = aimBounds.center;
        aimRadius_ = (std::max)(collisionRadius_, aimBounds.radius * maxScale);
    } else {
        aimLocalCenter_ = { 0.0f, 0.0f, 0.0f };
        aimRadius_ = collisionRadius_;
    }

    baseTranslate_ = position;
    translate_ = position;
    phase_ = position.x * 0.75f + position.y * 1.35f;
    object_->SetScale(baseScale_);
    object_->SetColor(baseColor_);
    object_->SetEnvironmentCoefficient(0.10f);
    object_->SetTranslate(translate_);
    object_->Update();
}

void Enemy::Update(float railDistance)
{
    if (IsDead() || !object_) {
        return;
    }

    ++age_;
    moveTimer_ += 1.0f;
    if (hitFlashTimer_ > 0) {
        --hitFlashTimer_;
    }

    switch (behavior_) {
    case Behavior::Swoop: {
        const float passRate = Clamp01(static_cast<float>(age_) / 210.0f);
        const float side =
            entryStyle_ == EntryStyle::RightSweep ? -1.0f :
            entryStyle_ == EntryStyle::LeftSweep ? 1.0f :
            SignNonZero(baseTranslate_.x);
        const float curve = std::sin(passRate * kPi);
        translate_.x =
            side * Lerp(8.8f, -10.2f, passRate) -
            side * curve * 1.8f;
        translate_.y =
            baseTranslate_.y +
            curve * 2.5f -
            passRate * 1.2f;
        translate_.z =
            railDistance +
            Lerp(32.0f, 21.0f, curve) +
            passRate * 7.0f;
        if (passRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::StrafeShooter: {
        const float attackRate = Clamp01(static_cast<float>(age_) / 90.0f);
        const float holdRate = Clamp01(static_cast<float>(age_ - 90) / 150.0f);
        const float exitRate = Clamp01(static_cast<float>(age_ - 240) / 110.0f);
        const float exitSide = SignNonZero(std::sin(phase_));
        const float entryArc =
            entryStyle_ == EntryStyle::PopShooter ?
            std::sin(attackRate * kPi) :
            0.0f;
        translate_.x =
            Lerp(baseTranslate_.x * 1.45f, baseTranslate_.x, attackRate) +
            std::sin(moveTimer_ * 0.045f + phase_) * Lerp(1.5f, 4.0f, holdRate) +
            exitSide * exitRate * 10.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 2.4f, baseTranslate_.y * 0.35f, attackRate) +
            std::cos(moveTimer_ * 0.038f + phase_) * 0.35f +
            entryArc * 0.7f +
            exitRate * 4.2f;
        translate_.z =
            railDistance +
            Lerp(34.0f, 21.0f, attackRate) +
            std::cos(moveTimer_ * 0.028f + phase_) * 1.0f +
            exitRate * 10.0f;
        if (exitRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::Formation:
    default:
    {
        const float entryRate = Clamp01(static_cast<float>(age_) / 90.0f);
        const float holdRate = Clamp01(static_cast<float>(age_ - 90) / 130.0f);
        const float exitRate = Clamp01(static_cast<float>(age_ - 220) / 120.0f);
        const float exitSide = SignNonZero(baseTranslate_.x);
        const float formationSpread =
            entryStyle_ == EntryStyle::VFormation ?
            std::sin(entryRate * kPi) * std::abs(baseTranslate_.x) * 0.35f :
            0.0f;
        translate_.x =
            Lerp(baseTranslate_.x * 0.55f, baseTranslate_.x, entryRate) +
            SignNonZero(baseTranslate_.x) * formationSpread +
            std::sin(moveTimer_ * 0.035f + phase_) * 0.75f +
            std::sin(moveTimer_ * 0.013f + phase_) * 0.35f +
            exitSide * exitRate * 10.5f;
        translate_.y =
            baseTranslate_.y +
            std::cos(moveTimer_ * 0.03f + phase_) * 0.45f +
            exitRate * 3.4f;
        translate_.z =
            railDistance +
            Lerp(38.0f, 24.0f, entryRate) -
            holdRate * 0.8f +
            std::sin(moveTimer_ * 0.02f + phase_) * 1.2f;
        if (exitRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    }

    const bool hitReacting = hitFlashTimer_ > 0;
    const float hitRate = hitReacting ? static_cast<float>(hitFlashTimer_) / 32.0f : 0.0f;
    const float hitPunch = hitReacting ? std::sin(hitRate * kPi) * 0.14f : 0.0f;
    const float flashScale = 1.0f + hitPunch;
    object_->SetScale({
        baseScale_.x * flashScale,
        baseScale_.y * flashScale,
        baseScale_.z * flashScale
    });
    object_->SetColor(baseColor_);
    object_->SetTranslate(translate_);
    object_->Update();
}

void Enemy::Draw()
{
    const bool blinkOff = hitFlashTimer_ > 0 && hitFlashTimer_ % 8 < 2;
    if (blinkOff) {
        return;
    }

    if (lifeState_ != LifeState::Destroyed &&
        lifeState_ != LifeState::Escaped &&
        object_) {
        object_->Draw();
    }
}

void Enemy::Kill()
{
    if (lifeState_ != LifeState::Alive) {
        return;
    }

    lifeState_ = LifeState::Destroyed;
}

bool Enemy::Damage(int damage)
{
    if (lifeState_ != LifeState::Alive) {
        return false;
    }

    hp_ = (std::max)(0, hp_ - (std::max)(damage, 0));
    hitFlashTimer_ = 32;
    if (hp_ <= 0) {
        Kill();
        return true;
    }
    return false;
}

Math::Vector3 Enemy::GetAimPosition() const
{
    return {
        translate_.x + aimLocalCenter_.x * baseScale_.x,
        translate_.y + aimLocalCenter_.y * baseScale_.y,
        translate_.z + aimLocalCenter_.z * baseScale_.z
    };
}

bool Enemy::CanShoot() const
{
    return lifeState_ == LifeState::Alive &&
        behavior_ == Behavior::StrafeShooter &&
        45 <= age_ &&
        age_ <= 220;
}
