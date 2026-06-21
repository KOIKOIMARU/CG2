#include "app/Bullet.h"

#include <algorithm>
#include <cmath>

void Bullet::Initialize(
    Object3dCommon* object3dCommon,
    Model* model,
    const Math::Vector3& position,
    const Math::Vector3& velocity,
    const Math::Vector4& color,
    int lifeTimer,
    const Math::Vector3& scale,
    float collisionRadius,
    int hitLimit,
    Model* glowModel,
    const Math::Vector4& glowColor,
    const Math::Vector3& glowScale,
    Model* trailModel,
    const Math::Vector4& trailColor,
    const Math::Vector3& trailScale,
    float trailOffset)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    bodyBaseScale_ = scale;
    bodyBaseColor_ = color;
    glowBaseScale_ = glowScale;
    glowBaseColor_ = glowColor;
    trailBaseScale_ = trailScale;
    trailBaseColor_ = trailColor;
    trailDistance_ = trailOffset;
    age_ = 0;
    initialLifeTimer_ = lifeTimer;
    object_->SetScale(bodyBaseScale_);
    object_->SetColor(bodyBaseColor_);

    translate_ = position;
    startTranslate_ = position;
    velocity_ = velocity;
    lifeTimer_ = lifeTimer;
    collisionRadius_ = collisionRadius;
    remainingHits_ = hitLimit;
    const float velocityLength =
        std::sqrt(
            velocity_.x * velocity_.x +
            velocity_.y * velocity_.y +
            velocity_.z * velocity_.z);
    if (velocityLength > 0.001f) {
        trailOffset_ = {
            -velocity_.x / velocityLength * trailDistance_,
            -velocity_.y / velocityLength * trailDistance_,
            -velocity_.z / velocityLength * trailDistance_
        };
    }
    if (std::abs(velocity_.x) > 0.001f || std::abs(velocity_.y) > 0.001f) {
        trailRoll_ = std::atan2(-velocity_.x, velocity_.y);
    } else {
        trailRoll_ = 0.0f;
    }
    object_->SetTranslate(translate_);
    object_->Update();

    if (glowModel) {
        glowObject_ = std::make_unique<Object3d>();
        glowObject_->Initialize(object3dCommon);
        glowObject_->SetModel(glowModel);
        glowObject_->SetScale(glowBaseScale_);
        glowObject_->SetColor(glowBaseColor_);
        glowObject_->SetLightingMode(0);
        glowObject_->SetEnvironmentCoefficient(0.0f);
        glowObject_->SetTranslate(translate_);
        glowObject_->Update();
    }
    if (trailModel) {
        trailObject_ = std::make_unique<Object3d>();
        trailObject_->Initialize(object3dCommon);
        trailObject_->SetModel(trailModel);
        trailObject_->SetScale(trailBaseScale_);
        trailObject_->SetColor(trailBaseColor_);
        trailObject_->SetLightingMode(0);
        trailObject_->SetEnvironmentCoefficient(0.0f);
        trailObject_->SetTranslate({
            translate_.x + trailOffset_.x,
            translate_.y + trailOffset_.y,
            translate_.z + trailOffset_.z
        });
        trailObject_->Update();

        for (auto& echoObject : trailEchoObjects_) {
            echoObject = std::make_unique<Object3d>();
            echoObject->Initialize(object3dCommon);
            echoObject->SetModel(trailModel);
            echoObject->SetScale(trailBaseScale_);
            echoObject->SetColor(trailBaseColor_);
            echoObject->SetLightingMode(0);
            echoObject->SetEnvironmentCoefficient(0.0f);
            echoObject->SetTranslate(translate_);
            echoObject->Update();
        }
    }
}

void Bullet::Update()
{
    if (isDead_ || !object_) {
        return;
    }

    ++age_;
    if (homingEnabled_ && hasHomingTarget_) {
        const float speed =
            std::sqrt(
                velocity_.x * velocity_.x +
                velocity_.y * velocity_.y +
                velocity_.z * velocity_.z);
        if (speed > 0.001f) {
            const Math::Vector3 currentDirection = Math::Normalize(velocity_);
            const Math::Vector3 targetDirection = Math::Normalize({
                homingTarget_.x - translate_.x,
                homingTarget_.y - translate_.y,
                homingTarget_.z - translate_.z
            });
            const float turn = std::clamp(homingStrength_, 0.0f, 1.0f);
            const Math::Vector3 steeredDirection = Math::Normalize({
                currentDirection.x + (targetDirection.x - currentDirection.x) * turn,
                currentDirection.y + (targetDirection.y - currentDirection.y) * turn,
                currentDirection.z + (targetDirection.z - currentDirection.z) * turn
            });
            velocity_ = steeredDirection * speed;
        }
    }
    translate_.x += velocity_.x;
    translate_.y += velocity_.y;
    translate_.z += velocity_.z;
    const float currentVelocityLength =
        std::sqrt(
            velocity_.x * velocity_.x +
            velocity_.y * velocity_.y +
            velocity_.z * velocity_.z);
    if (currentVelocityLength > 0.001f) {
        trailOffset_ = {
            -velocity_.x / currentVelocityLength * trailDistance_,
            -velocity_.y / currentVelocityLength * trailDistance_,
            -velocity_.z / currentVelocityLength * trailDistance_
        };
        if (std::abs(velocity_.x) > 0.001f || std::abs(velocity_.y) > 0.001f) {
            trailRoll_ = std::atan2(-velocity_.x, velocity_.y);
        }
    }
    object_->SetTranslate(translate_);
    const float corePulse = 1.0f + 0.04f * std::sin(static_cast<float>(age_) * 0.48f);
    object_->SetScale({
        bodyBaseScale_.x * corePulse,
        bodyBaseScale_.y * corePulse,
        bodyBaseScale_.z
    });
    object_->SetColor(bodyBaseColor_);
    object_->Update();
    if (glowObject_) {
        glowObject_->SetTranslate(translate_);
        glowObject_->Update();
    }
    if (trailObject_) {
        trailObject_->SetTranslate({
            translate_.x + trailOffset_.x,
            translate_.y + trailOffset_.y,
            translate_.z + trailOffset_.z
        });
        trailObject_->Update();
    }

    --lifeTimer_;
    const float dx = translate_.x - startTranslate_.x;
    const float dy = translate_.y - startTranslate_.y;
    const float dz = translate_.z - startTranslate_.z;
    const float travelDistanceSq = dx * dx + dy * dy + dz * dz;
    if (lifeTimer_ <= 0 ||
        travelDistanceSq > maxTravelDistance_ * maxTravelDistance_) {
        isDead_ = true;
    }
}

void Bullet::Draw()
{
    if (!isDead_ && object_) {
        object_->Draw();
    }
}

void Bullet::DrawGlow(const Math::Vector3& cameraRotate)
{
    const float lifeRate =
        initialLifeTimer_ > 0 ?
        static_cast<float>(lifeTimer_) / static_cast<float>(initialLifeTimer_) :
        1.0f;
    const float flicker = 0.92f + 0.08f * std::sin(static_cast<float>(age_) * 0.58f);
    const float tailBreath = 0.90f + 0.10f * std::sin(static_cast<float>(age_) * 0.34f);

    if (!isDead_ && trailObject_) {
        Math::Vector3 trailRotate = cameraRotate;
        const float trailFade = 0.58f + 0.42f * lifeRate;

        auto drawTrailNode = [&](
                                Object3d* trail,
                                float offsetScale,
                                float sizeScale,
                                float alphaScale) {
            if (!trail) {
                return;
            }
            trail->SetRotate(trailRotate);
            trail->SetScale({
                trailBaseScale_.x * tailBreath * sizeScale,
                trailBaseScale_.y * tailBreath * sizeScale,
                trailBaseScale_.z
            });
            Math::Vector4 trailColor = trailBaseColor_;
            trailColor.w *=
                (0.66f + 0.22f * flicker) *
                trailFade *
                alphaScale;
            trail->SetColor(trailColor);
            trail->SetTranslate({
                translate_.x + trailOffset_.x * offsetScale,
                translate_.y + trailOffset_.y * offsetScale,
                translate_.z + trailOffset_.z * offsetScale
            });
            trail->Update();
            trail->Draw();
        };

        drawTrailNode(trailEchoObjects_[2].get(), 3.15f, 1.65f, 0.24f);
        drawTrailNode(trailEchoObjects_[1].get(), 2.35f, 1.35f, 0.38f);
        drawTrailNode(trailEchoObjects_[0].get(), 1.65f, 1.10f, 0.58f);
        drawTrailNode(trailObject_.get(), 0.92f, 0.88f, 0.82f);
    }
    if (!isDead_ && glowObject_) {
        glowObject_->SetRotate(cameraRotate);
        glowObject_->SetScale({
            glowBaseScale_.x * flicker,
            glowBaseScale_.y * flicker,
            glowBaseScale_.z
        });
        Math::Vector4 glowColor = glowBaseColor_;
        glowColor.w *= (0.82f + 0.18f * flicker) * (0.65f + 0.35f * lifeRate);
        glowObject_->SetColor(glowColor);
        glowObject_->SetTranslate(translate_);
        glowObject_->Update();
        glowObject_->Draw();
    }
}

void Bullet::RegisterHit()
{
    --remainingHits_;
    if (remainingHits_ <= 0) {
        isDead_ = true;
    }
}

void Bullet::EnableHoming(float strength)
{
    homingEnabled_ = true;
    homingStrength_ = strength;
}

void Bullet::SetHomingTarget(const Math::Vector3& target)
{
    homingTarget_ = target;
    hasHomingTarget_ = true;
}
