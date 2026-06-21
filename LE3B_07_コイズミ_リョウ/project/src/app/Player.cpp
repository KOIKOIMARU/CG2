#include "app/Player.h"

#include "engine/io/Input.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace {
constexpr int kDodgeDuration = 18;
constexpr int kDodgeCooldown = 42;
constexpr int kDodgeInvincibleDuration = 16;
constexpr float kDodgeBaseSpeed = 0.14f;
constexpr float kDodgePeakSpeed = 0.08f;
} // namespace

void Player::Initialize(Object3dCommon* object3dCommon, Model* model)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    object_->SetScale({ 0.8f, 0.35f, 1.0f });
    object_->SetColor({ 0.25f, 0.65f, 1.0f, 1.0f });
    object_->SetTranslate(translate_);
    object_->Update();
}

void Player::Update(Input* input)
{
    if (!input || !object_ || IsDead()) {
        return;
    }

    if (dodgeCooldownTimer_ > 0) {
        --dodgeCooldownTimer_;
    }
    if (invincibleTimer_ > 0) {
        --invincibleTimer_;
    }

    Math::Vector3 move{ 0.0f, 0.0f, 0.0f };
    int horizontalInput = 0;
    if (input->PushKey(DIK_W) || input->PushKey(DIK_UP)) {
        move.y += moveSpeed_;
    }
    if (input->PushKey(DIK_S) || input->PushKey(DIK_DOWN)) {
        move.y -= moveSpeed_;
    }
    if (input->PushKey(DIK_D) || input->PushKey(DIK_RIGHT)) {
        move.x += moveSpeed_;
        horizontalInput += 1;
    }
    if (input->PushKey(DIK_A) || input->PushKey(DIK_LEFT)) {
        move.x -= moveSpeed_;
        horizontalInput -= 1;
    }
    if (horizontalInput != 0) {
        lastHorizontalDirection_ = horizontalInput > 0 ? 1 : -1;
    }

    const bool dodgeTriggered =
        input->TriggerKey(DIK_LSHIFT) || input->TriggerKey(DIK_RSHIFT);
    if (dodgeTriggered && dodgeCooldownTimer_ <= 0 && dodgeTimer_ <= 0) {
        dodgeDirection_ =
            horizontalInput != 0 ?
            (horizontalInput > 0 ? 1 : -1) :
            lastHorizontalDirection_;
        dodgeTimer_ = kDodgeDuration;
        dodgeCooldownTimer_ = kDodgeCooldown;
        invincibleTimer_ = kDodgeInvincibleDuration;
    }

    Math::Vector3 rotate{ 0.0f, 0.0f, 0.0f };
    if (dodgeTimer_ > 0) {
        const float progress =
            static_cast<float>(kDodgeDuration - dodgeTimer_) /
            static_cast<float>(kDodgeDuration);
        const float ease = std::sin(progress * std::numbers::pi_v<float>);
        move.x = 0.0f;
        move.x +=
            static_cast<float>(dodgeDirection_) *
            (kDodgeBaseSpeed + kDodgePeakSpeed * ease);
        rotate.z =
            static_cast<float>(dodgeDirection_) *
            progress *
            2.0f *
            std::numbers::pi_v<float>;
        --dodgeTimer_;
    }

    translate_.x = std::clamp(translate_.x + move.x, -5.5f, 5.5f);
    translate_.y = std::clamp(translate_.y + move.y, -3.0f, 3.0f);

    object_->SetTranslate(translate_);
    object_->SetRotate(rotate);
    object_->SetColor(
        invincibleTimer_ > 0 ?
        Math::Vector4{ 0.55f, 0.95f, 1.0f, 1.0f } :
        Math::Vector4{ 0.25f, 0.65f, 1.0f, 1.0f });
    object_->Update();
}

void Player::Draw()
{
    if (object_) {
        object_->Draw();
    }
}

void Player::SetRailZ(float z)
{
    translate_.z = z;
    if (object_) {
        object_->SetTranslate(translate_);
        object_->Update();
    }
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
