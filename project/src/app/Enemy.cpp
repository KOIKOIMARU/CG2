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

float EaseOutCubic(float value)
{
    const float t = Clamp01(value);
    const float inv = 1.0f - t;
    return 1.0f - inv * inv * inv;
}

float SmoothStep(float value)
{
    const float t = Clamp01(value);
    return t * t * (3.0f - 2.0f * t);
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
    visualScaleRate_ = 0.42f;

    switch (behavior_) {
    case Behavior::Swoop:
        baseScale_ = { 2.10f, 2.10f, 2.10f };
        baseColor_ = { 0.95f, 0.35f, 1.0f, 1.0f };
        maxHp_ = 2;
        horizontalAmplitude_ = 0.75f;
        verticalAmplitude_ = 0.35f;
        collisionRadius_ = 1.05f;
        break;
    case Behavior::StrafeShooter:
        baseScale_ = { 2.55f, 2.55f, 2.55f };
        baseColor_ = { 1.0f, 0.65f, 0.2f, 1.0f };
        maxHp_ = 3;
        horizontalAmplitude_ = 1.9f;
        verticalAmplitude_ = 0.25f;
        collisionRadius_ = 1.25f;
        break;
    case Behavior::Formation:
    default:
        baseScale_ = { 2.25f, 2.25f, 2.25f };
        baseColor_ = { 1.0f, 0.25f, 0.25f, 1.0f };
        maxHp_ = 2;
        collisionRadius_ = 1.05f;
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
    object_->SetEnvironmentCoefficient(0.0f);
    object_->SetTranslate(translate_);
    object_->SetRotate({ 0.0f, 0.0f, 0.0f });
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

    Math::Vector3 objectRotate{};
    float visualScaleTarget = 1.0f;
    float colorAlphaRate = 1.0f;

    switch (behavior_) {
    case Behavior::Swoop: {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 40.0f);
        const float passRate = SmoothStep(static_cast<float>(age_ - 14) / 182.0f);
        const float side =
            entryStyle_ == EntryStyle::RightSweep ? -1.0f :
            entryStyle_ == EntryStyle::LeftSweep ? 1.0f :
            SignNonZero(baseTranslate_.x);
        const float curve = std::sin(passRate * kPi);
        const float laneX =
            side * Lerp(10.8f, -11.6f, passRate) -
            side * curve * 2.8f;
        const float laneY =
            baseTranslate_.y +
            curve * 3.2f -
            passRate * 1.55f;
        const float laneZ =
            Lerp(36.0f, 17.5f, curve) +
            passRate * 9.0f;
        translate_.x =
            Lerp(side * 4.4f, laneX, entryRate);
        translate_.y =
            Lerp(baseTranslate_.y + 2.5f, laneY, entryRate) +
            std::sin(moveTimer_ * 0.064f + phase_) * 0.20f;
        translate_.z =
            railDistance +
            Lerp(58.0f, laneZ, entryRate);
        visualScaleTarget =
            Lerp(0.42f, 1.0f, entryRate) +
            std::sin(entryRate * kPi) * 0.05f;
        colorAlphaRate = Lerp(0.28f, 1.0f, entryRate);
        objectRotate.y = side * Lerp(0.35f, -0.25f, passRate);
        objectRotate.z = -side * (0.18f + curve * 0.40f);
        if (passRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::StrafeShooter: {
        const float attackRate = EaseOutCubic(static_cast<float>(age_) / 72.0f);
        const float holdRate = Clamp01(static_cast<float>(age_ - 72) / 148.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 220) / 96.0f);
        const float exitSide = SignNonZero(std::sin(phase_));
        const float entryArc =
            entryStyle_ == EntryStyle::PopShooter ?
            std::sin(attackRate * kPi) :
            0.0f;
        translate_.x =
            Lerp(
                baseTranslate_.x * 2.2f + SignNonZero(baseTranslate_.x) * 2.2f,
                baseTranslate_.x,
                attackRate) +
            std::sin(moveTimer_ * 0.050f + phase_) * Lerp(1.7f, 4.4f, holdRate) +
            exitSide * exitRate * 11.0f;
        translate_.y =
            Lerp(baseTranslate_.y + 3.3f, baseTranslate_.y * 0.30f, attackRate) +
            std::cos(moveTimer_ * 0.042f + phase_) * 0.42f +
            entryArc * 1.05f +
            exitRate * 4.8f;
        translate_.z =
            railDistance +
            Lerp(56.0f, 17.0f, attackRate) +
            std::cos(moveTimer_ * 0.030f + phase_) * 1.3f +
            exitRate * 11.0f;
        visualScaleTarget =
            Lerp(0.44f, 1.0f, attackRate) +
            std::sin(attackRate * kPi) * 0.06f;
        colorAlphaRate = Lerp(0.24f, 1.0f, attackRate);
        objectRotate.y = -translate_.x * 0.030f;
        objectRotate.z =
            std::sin(moveTimer_ * 0.032f + phase_) * 0.18f -
            exitSide * exitRate * 0.35f;
        if (exitRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    case Behavior::Formation:
    default:
    {
        const float entryRate = EaseOutCubic(static_cast<float>(age_) / 58.0f);
        const float holdRate = Clamp01(static_cast<float>(age_ - 58) / 132.0f);
        const float exitRate = SmoothStep(static_cast<float>(age_ - 206) / 96.0f);
        const float exitSide = SignNonZero(baseTranslate_.x);
        const float formationSpread =
            entryStyle_ == EntryStyle::VFormation ?
            std::sin(entryRate * kPi) * std::abs(baseTranslate_.x) * 0.56f :
            0.0f;
        translate_.x =
            Lerp(baseTranslate_.x * 0.08f, baseTranslate_.x, entryRate) +
            SignNonZero(baseTranslate_.x) * formationSpread +
            std::sin(moveTimer_ * 0.040f + phase_) * Lerp(0.20f, 0.95f, entryRate) +
            std::sin(moveTimer_ * 0.014f + phase_) * 0.42f +
            exitSide * exitRate * 11.5f;
        translate_.y =
            Lerp(baseTranslate_.y + 2.1f, baseTranslate_.y, entryRate) +
            std::cos(moveTimer_ * 0.034f + phase_) * 0.52f +
            exitRate * 4.1f;
        translate_.z =
            railDistance +
            Lerp(58.0f, 17.5f, entryRate) -
            holdRate * 1.6f +
            std::sin(moveTimer_ * 0.022f + phase_) * 1.5f;
        visualScaleTarget =
            Lerp(0.42f, 1.0f, entryRate) +
            std::sin(entryRate * kPi) * 0.055f;
        colorAlphaRate = Lerp(0.26f, 1.0f, entryRate);
        objectRotate.y = -translate_.x * 0.018f;
        objectRotate.z =
            -exitSide * std::sin(entryRate * kPi) * 0.22f +
            std::sin(moveTimer_ * 0.025f + phase_) * 0.10f;
        if (exitRate >= 1.0f) {
            Escape();
            return;
        }
        break;
    }
    }

    const bool hitReacting = hitFlashTimer_ > 0;
    const float hitRate = hitReacting ? static_cast<float>(hitFlashTimer_) / 18.0f : 0.0f;
    const float hitPunch = hitReacting ? std::sin(Clamp01(hitRate) * kPi) * 0.16f : 0.0f;
    const float flashScale = 1.0f + hitPunch;
    visualScaleRate_ = std::clamp(visualScaleTarget, 0.34f, 1.12f);
    Math::Vector4 color = baseColor_;
    color.w *= std::clamp(colorAlphaRate, 0.18f, 1.0f);
    if (hitReacting) {
        const float flash = std::clamp(hitRate * hitRate, 0.0f, 1.0f);
        color.x = Lerp(color.x, 1.0f, flash);
        color.y = Lerp(color.y, 1.0f, flash * 0.92f);
        color.z = Lerp(color.z, 0.92f, flash * 0.86f);
    }
    object_->SetScale({
        baseScale_.x * visualScaleRate_ * flashScale,
        baseScale_.y * visualScaleRate_ * flashScale,
        baseScale_.z * visualScaleRate_ * flashScale
    });
    object_->SetColor(color);
    object_->SetTranslate(translate_);
    object_->SetRotate(objectRotate);
    object_->Update();
}

void Enemy::Draw()
{
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
    hitFlashTimer_ = 18;
    if (hp_ <= 0) {
        Kill();
        return true;
    }
    return false;
}

bool Enemy::IsTargetable() const
{
    return lifeState_ == LifeState::Alive && visualScaleRate_ >= 0.46f;
}

Math::Vector3 Enemy::GetAimPosition() const
{
    const float scaleRate = (std::max)(visualScaleRate_, 0.30f);
    return {
        translate_.x + aimLocalCenter_.x * baseScale_.x * scaleRate,
        translate_.y + aimLocalCenter_.y * baseScale_.y * scaleRate,
        translate_.z + aimLocalCenter_.z * baseScale_.z * scaleRate
    };
}

float Enemy::GetAimRadius() const
{
    return aimRadius_ * (std::max)(visualScaleRate_, 0.48f);
}

bool Enemy::CanShoot() const
{
    if (!IsTargetable()) {
        return false;
    }

    switch (behavior_) {
    case Behavior::StrafeShooter:
        return 48 <= age_ && age_ <= 250;
    case Behavior::Swoop:
        return 46 <= age_ && age_ <= 160;
    case Behavior::Formation:
    default:
        return 54 <= age_ && age_ <= 210;
    }
}
