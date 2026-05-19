#include "app/Bullet.h"

void Bullet::Initialize(
    Object3dCommon* object3dCommon,
    Model* model,
    const Math::Vector3& position,
    const Math::Vector3& velocity,
    const Math::Vector4& color,
    int lifeTimer)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    object_->SetScale({ 0.18f, 0.18f, 0.5f });
    object_->SetColor(color);

    translate_ = position;
    velocity_ = velocity;
    lifeTimer_ = lifeTimer;
    object_->SetTranslate(translate_);
    object_->Update();
}

void Bullet::Update()
{
    if (isDead_ || !object_) {
        return;
    }

    translate_.x += velocity_.x;
    translate_.y += velocity_.y;
    translate_.z += velocity_.z;
    object_->SetTranslate(translate_);
    object_->Update();

    --lifeTimer_;
    if (lifeTimer_ <= 0 || translate_.z > 45.0f || translate_.z < -18.0f) {
        isDead_ = true;
    }
}

void Bullet::Draw()
{
    if (!isDead_ && object_) {
        object_->Draw();
    }
}
