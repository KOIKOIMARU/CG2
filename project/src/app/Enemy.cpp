#include "app/Enemy.h"

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

    translate_ = position;
    object_->SetTranslate(translate_);
    object_->Update();
}

void Enemy::Update()
{
    if (isDead_ || !object_) {
        return;
    }

    translate_.z -= speed_;
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
