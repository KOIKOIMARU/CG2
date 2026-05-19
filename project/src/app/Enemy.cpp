#include "app/Enemy.h"

#include <cmath>

void Enemy::Initialize(
    Object3dCommon* object3dCommon,
    Model* model,
    const Math::Vector3& position)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    object_->SetScale({ 0.9f, 0.9f, 0.9f });
    object_->SetColor({ 1.0f, 0.25f, 0.25f, 1.0f });

    baseTranslate_ = position;
    translate_ = position;
    phase_ = position.x * 0.75f + position.y * 1.35f;
    object_->SetTranslate(translate_);
    object_->Update();
}

void Enemy::Update()
{
    if (isDead_ || !object_) {
        return;
    }

    moveTimer_ += 0.06f;
    baseTranslate_.z -= speed_;
    translate_.x =
        baseTranslate_.x + std::sin(moveTimer_ * 1.8f + phase_) * horizontalAmplitude_;
    translate_.y =
        baseTranslate_.y + std::cos(moveTimer_ * 1.25f + phase_) * verticalAmplitude_;
    translate_.z = baseTranslate_.z;
    if (translate_.z < -8.0f) {
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
