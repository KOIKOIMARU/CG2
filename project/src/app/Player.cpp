#include "app/Player.h"

#include "engine/io/Input.h"

#include <algorithm>

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

    Math::Vector3 move{ 0.0f, 0.0f, 0.0f };
    if (input->PushKey(DIK_W) || input->PushKey(DIK_UP)) {
        move.y += moveSpeed_;
    }
    if (input->PushKey(DIK_S) || input->PushKey(DIK_DOWN)) {
        move.y -= moveSpeed_;
    }
    if (input->PushKey(DIK_D) || input->PushKey(DIK_RIGHT)) {
        move.x += moveSpeed_;
    }
    if (input->PushKey(DIK_A) || input->PushKey(DIK_LEFT)) {
        move.x -= moveSpeed_;
    }

    translate_.x = std::clamp(translate_.x + move.x, -5.5f, 5.5f);
    translate_.y = std::clamp(translate_.y + move.y, -3.0f, 3.0f);

    object_->SetTranslate(translate_);
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
