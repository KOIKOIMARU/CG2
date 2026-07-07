#include "app/Bullet.h"

#include <algorithm>
#include <cmath>

namespace {

Math::Vector3 TransformDirection(
    const Math::Vector3& direction,
    const Math::Matrix4x4& matrix)
{
    return {
        direction.x * matrix.m[0][0] +
            direction.y * matrix.m[1][0] +
            direction.z * matrix.m[2][0],
        direction.x * matrix.m[0][1] +
            direction.y * matrix.m[1][1] +
            direction.z * matrix.m[2][1],
        direction.x * matrix.m[0][2] +
            direction.y * matrix.m[1][2] +
            direction.z * matrix.m[2][2],
    };
}

float Dot(const Math::Vector3& a, const Math::Vector3& b)
{
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float CalculateBillboardRoll(
    const Math::Vector3& velocity,
    const Math::Vector3& cameraRotate,
    float fallbackRoll)
{
    const Math::Matrix4x4 cameraMatrix =
        Math::Multiply(
            Math::MakeRotateXMatrix(cameraRotate.x),
            Math::Multiply(
                Math::MakeRotateYMatrix(cameraRotate.y),
                Math::MakeRotateZMatrix(cameraRotate.z)));
    const Math::Vector3 cameraRight =
        TransformDirection({ 1.0f, 0.0f, 0.0f }, cameraMatrix);
    const Math::Vector3 cameraUp =
        TransformDirection({ 0.0f, 1.0f, 0.0f }, cameraMatrix);
    const float projectedX = Dot(velocity, cameraRight);
    const float projectedY = Dot(velocity, cameraUp);
    if (std::abs(projectedX) <= 0.001f && std::abs(projectedY) <= 0.001f) {
        return fallbackRoll;
    }
    return std::atan2(-projectedX, projectedY);
}

} // namespace

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
    float trailOffset,
    Model* sparkleModel,
    const Math::Vector4& sparkleColor,
    const Math::Vector3& sparkleScale)
{
    if (!object_) {
        object_ = std::make_unique<Object3d>();
        object_->Initialize(object3dCommon);
    }
    object_->SetModel(model);
    bodyBaseScale_ = scale;
    bodyBaseColor_ = color;
    glowBaseScale_ = glowScale;
    glowBaseColor_ = glowColor;
    trailBaseScale_ = trailScale;
    trailBaseColor_ = trailColor;
    sparkleBaseColor_ = sparkleColor;
    sparkleBaseScale_ = sparkleScale;
    trailDistance_ = trailOffset;
    age_ = 0;
    initialLifeTimer_ = lifeTimer;
    object_->SetScale(bodyBaseScale_);
    object_->SetColor(bodyBaseColor_);
    object_->SetLightingMode(0);
    object_->SetEnvironmentCoefficient(0.0f);
    object_->SetRotate({});

    translate_ = position;
    startTranslate_ = position;
    velocity_ = velocity;
    lifeTimer_ = lifeTimer;
    homingStrength_ = 0.0f;
    homingEnabled_ = false;
    hasHomingTarget_ = false;
    homingTarget_ = {};
    isDead_ = false;
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
        if (!glowObject_) {
            glowObject_ = std::make_unique<Object3d>();
            glowObject_->Initialize(object3dCommon);
        }
        glowObject_->SetModel(glowModel);
        glowObject_->SetScale(glowBaseScale_);
        glowObject_->SetColor(glowBaseColor_);
        glowObject_->SetLightingMode(0);
        glowObject_->SetEnvironmentCoefficient(0.0f);
        glowObject_->SetRotate({ 0.0f, 0.0f, trailRoll_ });
        glowObject_->SetTranslate(translate_);
        glowObject_->Update();
    } else {
        glowObject_.reset();
    }
    if (trailModel) {
        if (!trailObject_) {
            trailObject_ = std::make_unique<Object3d>();
            trailObject_->Initialize(object3dCommon);
        }
        trailObject_->SetModel(trailModel);
        trailObject_->SetScale(trailBaseScale_);
        trailObject_->SetColor(trailBaseColor_);
        trailObject_->SetLightingMode(0);
        trailObject_->SetEnvironmentCoefficient(0.0f);
        trailObject_->SetRotate({ 0.0f, 0.0f, trailRoll_ });
        trailObject_->SetTranslate({
            translate_.x + trailOffset_.x,
            translate_.y + trailOffset_.y,
            translate_.z + trailOffset_.z
        });
        trailObject_->Update();

    } else {
        trailObject_.reset();
    }
    if (sparkleModel) {
        for (size_t index = 0; index < sparkleObjects_.size(); ++index) {
            auto& sparkleObject = sparkleObjects_[index];
            if (!sparkleObject) {
                sparkleObject = std::make_unique<Object3d>();
                sparkleObject->Initialize(object3dCommon);
            }
            sparkleObject->SetModel(sparkleModel);
            sparkleObject->SetScale(sparkleBaseScale_);
            sparkleObject->SetColor(sparkleBaseColor_);
            sparkleObject->SetLightingMode(0);
            sparkleObject->SetEnvironmentCoefficient(0.0f);
            sparkleObject->SetRotate({ 0.0f, 0.0f, trailRoll_ });
            const float indexF = static_cast<float>(index);
            const float offsetScale = 0.95f + indexF * 0.72f;
            sparkleObject->SetTranslate({
                translate_.x + trailOffset_.x * offsetScale,
                translate_.y + trailOffset_.y * offsetScale,
                translate_.z + trailOffset_.z * offsetScale
            });
            sparkleObject->Update();
        }
    } else {
        for (auto& sparkleObject : sparkleObjects_) {
            sparkleObject.reset();
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
    const float flicker = 0.93f + 0.07f * std::sin(static_cast<float>(age_) * 0.58f);
    const float tailBreath = 0.99f + 0.09f * std::sin(static_cast<float>(age_) * 0.34f);

    if (!isDead_ && trailObject_) {
        Math::Vector3 trailRotate = cameraRotate;
        const float billboardRoll =
            CalculateBillboardRoll(velocity_, cameraRotate, trailRoll_);
        trailRotate.z += billboardRoll;
        const float trailFade = 0.74f + 0.26f * lifeRate;

        trailObject_->SetRotate(trailRotate);
        trailObject_->SetScale({
            trailBaseScale_.x * tailBreath * 0.78f,
            trailBaseScale_.y * tailBreath * 0.78f,
            trailBaseScale_.z
        });
        Math::Vector4 trailColor = trailBaseColor_;
        trailColor.w *=
            (0.92f + 0.16f * flicker) *
            trailFade *
            0.82f;
        trailObject_->SetColor(trailColor);
        trailObject_->SetTranslate({
            translate_.x + trailOffset_.x * 0.56f,
            translate_.y + trailOffset_.y * 0.56f,
            translate_.z + trailOffset_.z * 0.56f
        });
        trailObject_->Update();
        trailObject_->Draw();
    }
    if (!isDead_ && glowObject_) {
        Math::Vector3 glowRotate = cameraRotate;
        glowRotate.z += CalculateBillboardRoll(velocity_, cameraRotate, trailRoll_);
        glowObject_->SetRotate(glowRotate);
        const float glowPunch = 1.0f + 0.05f * std::sin(static_cast<float>(age_) * 0.78f);
        glowObject_->SetScale({
            glowBaseScale_.x * flicker * glowPunch,
            glowBaseScale_.y * flicker * glowPunch,
            glowBaseScale_.z
        });
        Math::Vector4 glowColor = glowBaseColor_;
        glowColor.w *= (1.08f + 0.18f * flicker) * (0.84f + 0.16f * lifeRate);
        glowObject_->SetColor(glowColor);
        glowObject_->SetTranslate(translate_);
        glowObject_->Update();
        glowObject_->Draw();
    }

    if (!isDead_) {
        for (size_t index = 0; index < sparkleObjects_.size(); ++index) {
            Object3d* sparkle = sparkleObjects_[index].get();
            if (!sparkle) {
                continue;
            }

            const float indexF = static_cast<float>(index);
            const float centerIndex =
                (static_cast<float>(sparkleObjects_.size()) - 1.0f) * 0.5f;
            const float phase = static_cast<float>(age_) * (0.56f + indexF * 0.045f) + indexF * 1.37f;
            const float lane = indexF - centerIndex;
            const float offsetScale = 0.95f + indexF * 0.72f;
            const float side = std::sin(phase) * (0.058f + indexF * 0.0060f);
            const float up = std::cos(phase * 0.83f) * (0.048f + indexF * 0.0050f);
            const float sparklePulse = 0.78f + 0.30f * std::sin(phase * 1.31f);
            const float fade = (1.0f - indexF * 0.040f) * (0.86f + 0.14f * lifeRate);

            sparkle->SetRotate({
                cameraRotate.x,
                cameraRotate.y,
                cameraRotate.z + phase * 0.45f
            });
            sparkle->SetScale({
                sparkleBaseScale_.x * sparklePulse * (0.96f + indexF * 0.052f),
                sparkleBaseScale_.y * sparklePulse * (0.96f + indexF * 0.052f),
                sparkleBaseScale_.z
            });
            Math::Vector4 sparkleColor = sparkleBaseColor_;
            sparkleColor.w *= fade * (0.88f + 0.34f * sparklePulse);
            sparkle->SetColor(sparkleColor);
            sparkle->SetTranslate({
                translate_.x + trailOffset_.x * offsetScale + side + lane * 0.008f,
                translate_.y + trailOffset_.y * offsetScale + up,
                translate_.z + trailOffset_.z * offsetScale
            });
            sparkle->Update();
            sparkle->Draw();
        }
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
