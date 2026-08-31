#include "samples/rail_shooter/Player.h"

#include "engine/3d/Model.h"
#include "engine/io/Input.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace {
constexpr int kDodgeDuration = 18;
constexpr int kDodgeCooldown = 34;
constexpr int kDodgeInvincibleDuration = 17;
constexpr float kDodgeBaseSpeed = 0.24f;
constexpr float kDodgePeakSpeed = 0.18f;
constexpr float kDodgeSlowMinScale = 0.18f;
constexpr float kPlayerHorizontalLimit = 8.9f;
constexpr float kPlayerLowerLimitY = -0.80f;
constexpr float kPlayerUpperLimitY = 5.55f;
constexpr float kPlayerMoveBankAngle = 0.34f;
constexpr float kPlayerMoveYawAngle = 0.055f;
constexpr float kPlayerMovePitchAngle = 0.075f;
constexpr float kPlayerPoseResponse = 0.17f;

Math::Vector3 CalculateModelLocalCenterOffset(const Model* model)
{
    if (!model || model->GetVertices().empty()) {
        return { 0.0f, 0.0f, 0.0f };
    }

    Math::Vector3 minPosition{
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
        (std::numeric_limits<float>::max)(),
    };
    Math::Vector3 maxPosition{
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)(),
        -(std::numeric_limits<float>::max)(),
    };

    for (const VertexData& vertex : model->GetVertices()) {
        minPosition.x = (std::min)(minPosition.x, vertex.position.x);
        minPosition.y = (std::min)(minPosition.y, vertex.position.y);
        minPosition.z = (std::min)(minPosition.z, vertex.position.z);
        maxPosition.x = (std::max)(maxPosition.x, vertex.position.x);
        maxPosition.y = (std::max)(maxPosition.y, vertex.position.y);
        maxPosition.z = (std::max)(maxPosition.z, vertex.position.z);
    }

    return {
        (minPosition.x + maxPosition.x) * 0.5f,
        (minPosition.y + maxPosition.y) * 0.5f,
        (minPosition.z + maxPosition.z) * 0.5f,
    };
}

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
} // namespace

void Player::Initialize(Object3dCommon* object3dCommon, Model* model)
{
    objectRotate_ = {};
    movementRotate_ = {};
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    object_->SetScale(objectScale_);
    object_->SetColor({ 0.75f, 0.95f, 1.0f, 1.0f });
    object_->SetEnvironmentCoefficient(0.13f);
    object_->SetShininess(56.0f);
    object_->SetSpecularColor({ 0.66f, 0.78f, 0.88f });
    object_->SetRoughness(0.34f);
    object_->SetMetallic(0.28f);

    modelLocalCenterOffset_ = CalculateModelLocalCenterOffset(model);
    UpdateObjectTransform();
}

void Player::Update(Input* input, float timeScale)
{
    if (!input || !object_ || IsDead()) {
        return;
    }

    const float motionScale = std::clamp(timeScale, 0.10f, 1.0f);
    const float dodgeTimeStep = std::clamp(timeScale, kDodgeSlowMinScale, 1.0f);

    if (dodgeCooldownTimer_ > 0) {
        --dodgeCooldownTimer_;
    }
    if (invincibleTimer_ > 0) {
        --invincibleTimer_;
    }

    Math::Vector3 inputMove{ 0.0f, 0.0f, 0.0f };
    Math::Vector3 dodgeMove{ 0.0f, 0.0f, 0.0f };
    int horizontalInput = 0;
    int verticalInput = 0;
    if (input->PushKey(DIK_W) || input->PushKey(DIK_UP)) {
        inputMove.y += moveSpeed_;
        verticalInput += 1;
    }
    if (input->PushKey(DIK_S) || input->PushKey(DIK_DOWN)) {
        inputMove.y -= moveSpeed_;
        verticalInput -= 1;
    }
    if (input->PushKey(DIK_D) || input->PushKey(DIK_RIGHT)) {
        inputMove.x += moveSpeed_;
        horizontalInput += 1;
    }
    if (input->PushKey(DIK_A) || input->PushKey(DIK_LEFT)) {
        inputMove.x -= moveSpeed_;
        horizontalInput -= 1;
    }
    if (horizontalInput != 0) {
        lastHorizontalDirection_ = horizontalInput > 0 ? 1 : -1;
    }

    const bool dodgeTriggered =
        input->TriggerKey(DIK_LSHIFT) || input->TriggerKey(DIK_RSHIFT);
    if (dodgeTriggered && dodgeCooldownTimer_ <= 0 && dodgeTimer_ <= 0.0f) {
        dodgeDirection_ =
            horizontalInput != 0 ?
            (horizontalInput > 0 ? 1 : -1) :
            lastHorizontalDirection_;
        dodgeTimer_ = static_cast<float>(kDodgeDuration);
        dodgeCooldownTimer_ = kDodgeCooldown;
        invincibleTimer_ = kDodgeInvincibleDuration;
    }

    const bool hasDodgePose = dodgeTimer_ > 0.0f;
    const int poseDirection = hasDodgePose ? dodgeDirection_ : horizontalInput;
    const Math::Vector3 targetMovementRotate{
        -static_cast<float>(verticalInput) * kPlayerMovePitchAngle,
        static_cast<float>(poseDirection) * kPlayerMoveYawAngle,
        -static_cast<float>(poseDirection) * kPlayerMoveBankAngle,
    };
    const float poseBlend =
        1.0f - std::pow(1.0f - kPlayerPoseResponse, motionScale);
    movementRotate_.x +=
        (targetMovementRotate.x - movementRotate_.x) * poseBlend;
    movementRotate_.y +=
        (targetMovementRotate.y - movementRotate_.y) * poseBlend;
    movementRotate_.z +=
        (targetMovementRotate.z - movementRotate_.z) * poseBlend;

    Math::Vector3 rotate = movementRotate_;
    if (dodgeTimer_ > 0.0f) {
        const float progress =
            (static_cast<float>(kDodgeDuration) - dodgeTimer_) /
            static_cast<float>(kDodgeDuration);
        const float ease = std::sin(progress * std::numbers::pi_v<float>);
        inputMove.x = 0.0f;
        dodgeMove.x +=
            static_cast<float>(dodgeDirection_) *
            (kDodgeBaseSpeed + kDodgePeakSpeed * ease) * dodgeTimeStep;
        rotate.z +=
            static_cast<float>(dodgeDirection_) *
            progress *
            2.0f *
            std::numbers::pi_v<float>;
        dodgeTimer_ = (std::max)(dodgeTimer_ - dodgeTimeStep, 0.0f);
    }

    translate_.x =
        std::clamp(
            translate_.x + inputMove.x * motionScale + dodgeMove.x,
            -kPlayerHorizontalLimit,
            kPlayerHorizontalLimit);
    translate_.y =
        std::clamp(
            translate_.y + inputMove.y * motionScale + dodgeMove.y,
            kPlayerLowerLimitY,
            kPlayerUpperLimitY);

    objectRotate_ = rotate;
    object_->SetColor(
        invincibleTimer_ > 0 ?
        Math::Vector4{ 0.55f, 0.95f, 1.0f, 1.0f } :
        Math::Vector4{ 0.75f, 0.95f, 1.0f, 1.0f });
    UpdateObjectTransform();
}

void Player::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void Player::DrawShadow(const Math::Matrix4x4& lightViewProjection)
{
    if (object_ && !IsDead()) {
        object_->DrawShadow(lightViewProjection);
    }
}

void Player::SetRailZ(float z)
{
    translate_.z = z;
    if (object_) {
        UpdateObjectTransform();
    }
}

void Player::UpdateObjectTransform()
{
    if (!object_) {
        return;
    }

    const Math::Vector3 scaledCenterOffset{
        modelLocalCenterOffset_.x * objectScale_.x,
        modelLocalCenterOffset_.y * objectScale_.y,
        modelLocalCenterOffset_.z * objectScale_.z,
    };
    const Math::Matrix4x4 rotateMatrix =
        Math::Multiply(
            Math::MakeRotateXMatrix(objectRotate_.x),
            Math::Multiply(
                Math::MakeRotateYMatrix(objectRotate_.y),
                Math::MakeRotateZMatrix(objectRotate_.z)));
    const Math::Vector3 rotatedCenterOffset =
        TransformDirection(scaledCenterOffset, rotateMatrix);
    object_->SetTranslate({
        translate_.x - rotatedCenterOffset.x,
        translate_.y - rotatedCenterOffset.y,
        translate_.z - rotatedCenterOffset.z,
    });
    object_->SetRotate(objectRotate_);
    object_->Update();
}

void Player::Damage(int amount)
{
    hp_ -= amount;
    if (hp_ < 0) {
        hp_ = 0;
    }
    if (object_) {
        object_->SetColor(
            IsDead() ?
            Math::Vector4{ 0.35f, 0.35f, 0.4f, 1.0f } :
            Math::Vector4{ 1.0f, 0.35f, 0.35f, 1.0f });
    }
}
