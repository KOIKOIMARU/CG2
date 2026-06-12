#include "app/Bullet.h"

void Bullet::Initialize(
    Object3dCommon* object3dCommon,
    Model* model,
    const Math::Vector3& position,
    const Math::Vector3& velocity,
    const Math::Vector4& color,
    int lifeTimer,
    const Math::Vector3& scale,
    float collisionRadius,
    int hitLimit)
{
    object_ = std::make_unique<Object3d>();
    object_->Initialize(object3dCommon);
    object_->SetModel(model);
    object_->SetScale(scale);
    object_->SetColor(color);

    translate_ = position;
    startTranslate_ = position;
    velocity_ = velocity;
    lifeTimer_ = lifeTimer;
    collisionRadius_ = collisionRadius;
    remainingHits_ = hitLimit;
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

void Bullet::RegisterHit()
{
    --remainingHits_;
    if (remainingHits_ <= 0) {
        isDead_ = true;
    }
}
