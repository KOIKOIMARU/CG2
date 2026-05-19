#pragma once

#include "engine/3d/Object3d.h"
#include "engine/base/Math.h"

#include <memory>

class Model;
class Object3dCommon;

class Bullet {
public:
    void Initialize(
        Object3dCommon* object3dCommon,
        Model* model,
        const Math::Vector3& position,
        const Math::Vector3& velocity,
        const Math::Vector4& color = { 1.0f, 0.85f, 0.25f, 1.0f },
        int lifeTimer = 180);
    void Update();
    void Draw();

    bool IsDead() const { return isDead_; }
    void Kill() { isDead_ = true; }
    const Math::Vector3& GetTranslate() const { return translate_; }
    float GetRadius() const { return collisionRadius_; }

private:
    std::unique_ptr<Object3d> object_;
    Math::Vector3 translate_{};
    Math::Vector3 velocity_{ 0.0f, 0.0f, 0.5f };
    float collisionRadius_ = 0.35f;
    int lifeTimer_ = 180;
    bool isDead_ = false;
};
